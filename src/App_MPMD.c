#include "App_MPMD.h"

#include <string.h>

//! Description of a single component (i.e. executable) within an MPMD context.
//! It can describe components *other* than the one to which the current PE belongs.
typedef struct TComponent_ {
    int id;             //!< ID of this component
    MPI_Comm comm;      //!< Communicator for the PEs of this component
    int num_pes;        //!< How many PEs are members of this component
    int already_shared; //!< Whether this component has been shared to other PEs of this PE's component
    int* ranks;         //!< Global ranks (in the main_comm of the context) of the members of this component
 } TComponent;

//! A series of components that share a communicator
typedef struct TComponentSet_ {
    int num_components;     //!< How many components are in the set
    int* component_ids;     //!< IDs of the components in this set
    MPI_Comm communicator;  //!< Communicator shared by these components
    MPI_Group group;        //!< MPI group shared by these component, created for creating the communicator
} TComponentSet;

//!> Default values for a TComponent struct
static const TComponent default_component = (TComponent){
    .id             = APP_MPMD_NONE,
    .comm           = MPI_COMM_NULL,
    .num_pes        = 0,
    .already_shared = 0,
    .ranks          = NULL
};

//!> Default values for a TComponentSet struct
static const TComponentSet default_set = (TComponentSet) {
    .num_components = 0,
    .component_ids  = NULL,
    .communicator   = MPI_COMM_NULL,
    .group          = MPI_GROUP_NULL
};


// Prototypes
void print_component(const TComponent* comp, const int verbose);
void print_set(const TComponentSet* set);
const TComponentSet* find_set(const TApp* App, const int* Components, const int NumComponents);
const TComponentSet* create_set(TApp* App, const int* Components, const int NumComponents);

//! Initialize a common MPMD context by telling everyone who we are as a process.
//! This is a collective call. Everyone who participate in it will know who else
//! is on board and will be able to ask for a communicator in common with any other
//! participant (or even multiple other participants at once).
TApp* App_MPMD_Init(const char * const ComponentName, const char * const version) {
    #pragma omp single // For this entire function
    {
    // Start by initializing MPI
    const int required = MPI_THREAD_MULTIPLE;

    int provided;
    MPI_Init_thread(NULL, NULL, required, &provided);

    //! \fixme Shouldn't it be the version of the calling app?
    TApp * const app = App_Init(0, ComponentName, version, "mpmd context attempt", "now");
    App_Start();

    App_Log(APP_DEBUG, "%s: Initializing component %s\n", __func__, ComponentName);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &app->world_rank);

    if (provided != required) {
        if (app->world_rank == 0) {
            App_Log(APP_ERROR, "%s: In MPMD context initialization: your system does NOT support MPI_THREAD_MULTIPLE\n", __func__);
        }
        MPI_Finalize();
        exit(-1);
    }

    app->MainComm = MPI_COMM_WORLD;

    // We need to assign a unique id for the component, but multiple mpi processors may share the same component name
    // This needs to be a collective call : MPI_Gather()
    // In order to allocate our input buffer, we must set a maximum component name length
    {
        char component_names[world_size][MAX_COMPONENT_NAME_LEN];
        MPI_Gather(component_name, strlen(component_name), MPI_BYTE, component_names, );
    }

    // Determine which programs are present
    MPI_Comm component_comm;
    MPI_Comm_split(MPI_COMM_WORLD, ComponentId, app->WorldRank, &component_comm);
    MPI_Comm_rank(component_comm, &app->ComponentRank);

    // Declare that rank 0 of this component is "active" as a logger
    if (app->ComponentRank == 0) App_LogRank(app->WorldRank);

    App_Log(APP_INFO, "%s: Initializing component %s (ID %d)\n", __func__, component_id_to_name(ComponentId), ComponentId);

    if (app->WorldRank == 0 && app->ComponentRank != 0) {
        App_Log(APP_FATAL, "%s: Global root should also be the root of its own component\n", __func__);
    }

    // Initialize individual components
    MPI_Comm roots_comm = MPI_COMM_NULL;
    int component_pos = -1;
    if (app->ComponentRank == 0) {
        // Use component ID as key so that the components are sorted in ascending order
        MPI_Comm_split(MPI_COMM_WORLD, 0, ComponentId, &roots_comm);
        MPI_Comm_size(roots_comm, &app->NumComponents);
        MPI_Comm_rank(roots_comm, &component_pos);

        App_Log(APP_DEBUG, "%s: (%s) There are %d components present\n", __func__, component_name, app->NumComponents);
    } else {
        MPI_Comm dummy_comm;
        MPI_Comm_split(MPI_COMM_WORLD, 1, ComponentId, &dummy_comm);
    }

    // Transmit basic component information to every PE
    MPI_Bcast(&app->NumComponents, 1, MPI_INT, 0, MPI_COMM_WORLD);

    app->AllComponents = (TComponent*)malloc(app->NumComponents * sizeof(TComponent));
    for (int i = 0; i < app->NumComponents; i++) app->AllComponents[i] = default_component;

    // Each component root should have a list of the world rank of every other root
    // (we don't know which one is the WORLD root)
    int root_world_ranks[app->NumComponents];
    if (app->ComponentRank == 0) {
        MPI_Allgather(&app->WorldRank, 1, MPI_INT, root_world_ranks, 1, MPI_INT, roots_comm);
    }

    // Send the component roots to everyone
    MPI_Bcast(root_world_ranks, app->NumComponents, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&component_pos, 1, MPI_INT, 0, component_comm);

    // Select which component belongs to this PE and initialize its struct within this PE
    app->SelfComponent = &app->AllComponents[component_pos];
    app->SelfComponent->id = ComponentId;
    MPI_Comm_size(component_comm, &(app->SelfComponent->num_pes));

    // Send basic component information to everyone
    for (int i = 0; i < app->NumComponents; i++) {
        MPI_Bcast(&app->AllComponents[i], sizeof(TComponent), MPI_BYTE, root_world_ranks[i], MPI_COMM_WORLD);
    }

    // Self-component info that is only valid on a PE from this component
    app->SelfComponent->comm  = component_comm;
    app->SelfComponent->ranks = (int*)malloc(app->SelfComponent->num_pes * sizeof(int));


    // Determine global ranks of the PEs in this component and share with other components

    // Each component gathers the world ranks of its members
    MPI_Allgather(&app->WorldRank, 1, MPI_INTEGER, app->SelfComponent->ranks, 1, MPI_INT, component_comm);
    app->SelfComponent->already_shared = 1;

    // Every component root (one by one) sends the ranks of its members to every other component root
    // This way seems easier than to set up everything to be able to use MPI_Allgatherv
    // We only send the list of all other ranks to the roots (rather than aaalllll PEs) to avoid a bit
    // of communication and storage
    if (app->ComponentRank == 0) {
        for (int i = 0; i < app->NumComponents; i++) {
            if (app->AllComponents[i].ranks == NULL)
                app->AllComponents[i].ranks = (int*)malloc(app->AllComponents[i].num_pes * sizeof(int));

            MPI_Bcast(app->AllComponents[i].ranks, app->AllComponents[i].num_pes, MPI_INT, i, roots_comm);
        }
    }

    // Print some info about the components, for debugging
    if (app->WorldRank == 0) {
        App_Log(APP_DEBUG, "%s: Num components = %d\n", __func__, app->NumComponents);
        for (int i = 0; i < app->NumComponents; i++) {
            print_component(&app->AllComponents[i], 0);
        }
    }

    } // omp single

    return App_GetInstance();
}

//!> Terminate cleanly the MPMD run. This frees the memory allocated by TComponent and TComponentSet structs,
//!> and it calls MPI_Finalize()
void App_MPMD_Finalize(/* TApp* app */) {
    #pragma omp single
    {
        TApp* app = App_GetInstance();
        if (app->MainComm != MPI_COMM_NULL) {

            print_component(app->SelfComponent, 1);

            if (app->Sets != NULL) {
                for (int i = 0; i < app->NbSets; i++) {
                    free(app->Sets[i].component_ids);
                    MPI_Comm_free(&(app->Sets[i].communicator));
                    MPI_Group_free(&(app->Sets[i].group));
                }
                free(app->Sets);
                app->Sets = NULL;
            }
            app->NbSets = 0;

            for (int i = 0; i < app->NumComponents; i++) {
                if (app->AllComponents[i].ranks != NULL) {
                    App_Log(APP_DEBUG, "Freeing ranks %p\n", app->AllComponents[i].ranks);
                    free(app->AllComponents[i].ranks);
                }
            }
            MPI_Comm_free(&(app->SelfComponent->comm));
            free(app->AllComponents);
            app->AllComponents = NULL;
            app->SelfComponent = NULL;
            app->NumComponents = 0;

            MPI_Finalize();
            app->MainComm = MPI_COMM_NULL;
        }
    } // omp single
}

//!> \return 1 if a certain item is in the given list, 0 otherwise
int contains(
    const int* list,    //!< The list we want to search
    const int num_items,//!< How many items there are in the list
    const int item      //!< The item we are looking for in the list
) {
    for (int i = 0; i < num_items; i++) if (list[i] == item) return 1;
    return 0;
}


void print_list(int* list, int size) {
    if (App_LogLevel(NULL) >= APP_EXTRA) {
        char list_str[512];
        for (int i = 0; i < size; i++) sprintf(list_str + 8*i, "%7d ", list[i]);
        App_Log(APP_EXTRA, "List: %s\n", list_str);
    }
}

//!> From the given list of components, get the same list in ascending order and without any duplicate.
//!> \return Cleaned up list. Will need to be freed manually!
int* get_clean_component_list(
    const int* components,      //!< Initial list of component IDs
    const int num_components,   //!< How many components there are in the initial list
    int* actual_num_components  //!< [out] How many components there are in the cleaned-up list
) {

    int tmp_list[num_components + 1];

    // Perform an insertion sort, while checking for duplicates
    for (int i = 0; i < num_components + 1; i++) tmp_list[i] = -1;
    tmp_list[0] = components[0];
    int num_items = 1;
    App_Log(APP_EXTRA, "num_components = %d\n", num_components);
    print_list(tmp_list, num_components + 1);
    for (int i = 1; i < num_components; i++) {
        const int item = components[i];

        App_Log(APP_EXTRA, "i = %d, num_items = %d, item = %d\n", i, num_items, item);
        print_list(tmp_list, num_components + 1);
        App_Log(APP_EXTRA, "duplicate? %d\n", contains(tmp_list, num_items, item));

        if (contains(tmp_list, num_items, item)) continue; // Duplicate

        for (int j = i - 1; j >= 0; j--) { // Loop through sorted list (so far)
            App_Log(APP_EXTRA, "j = %d\n", j);
            if (tmp_list[j] > item) {
                // Not the right position yet
                tmp_list[j + 1] = tmp_list[j];
                App_Log(APP_EXTRA, "Changed list to... \n");
                print_list(tmp_list, num_components + 1);

                if (j == 0) {
                    // Last one, so we know the 1st slot is the right one
                    tmp_list[j] = item;
                    num_items++;

                    App_Log(APP_EXTRA, "Changed list again to... \n");
                    print_list(tmp_list, num_components + 1);
                }
            }
            else if (tmp_list[j] < item) {
                // Found the right position
                tmp_list[j + 1] = item;
                num_items++;

                App_Log(APP_EXTRA, "Correct pos, changed list to... \n");
                print_list(tmp_list, num_components + 1);
                break;
            }
        }
    }

    // Put in list with the right size
    int* clean_list = (int*)malloc(num_items * sizeof(int));
    memcpy(clean_list, tmp_list, num_items * sizeof(int));
    *actual_num_components = num_items;

    return clean_list;
}

//! \return The communicator for all PEs part of the same component as me.
MPI_Comm App_MPMD_GetOwnComm(void) {
    TApp* app = App_GetInstance();
    if (app) {
        return app->SelfComponent->comm;
    }

    return MPI_COMM_NULL;
}

//! \return The (Fortran) communicator for all PEs part of the same component as me.
MPI_Fint App_MPMD_GetOwnComm_F(void) {
    return MPI_Comm_c2f(App_MPMD_GetOwnComm());
}

//!> Retrieve a communicator that encompasses all PEs part of the components
//!> in the given list. If the communicator does not already exist, it will be created.
//!> _This function call is collective if and only if the communicator gets created._
MPI_Comm App_MPMD_GetSharedComm(
    //!> The list of components IDs for which we want a shared communicator.
    //!> This list *must* contain the component of the calling PE. It may contain
    //!> duplicate IDs and does not have to be in a specific order.
    const int32_t* Components,
    const int32_t NumComponents
) {
    MPI_Comm shared_comm = MPI_COMM_NULL;

    TApp* app = App_GetInstance();

    // Sanitize the list of components
    int actual_num_components = 0;
    int* sorted_components = get_clean_component_list(Components, NumComponents, &actual_num_components);

    char comps[256] = {0};
    char clean_comps[256] = {0};
    for (int i = 0; i < 10 && i < NumComponents; i++) sprintf(comps + 8*i, " %7d", Components[i]);
    for (int i = 0; i < 10 && i < actual_num_components; i++) sprintf(clean_comps + 8*i, " %7d", sorted_components[i]);
    App_Log(APP_DEBUG, "%s: Retrieving shared comm for components %s (%s)\n", __func__, comps, clean_comps);

    // Make sure there are enough components in the list
    if (actual_num_components < 2) {
        App_Log(APP_ERROR, "%s: There need to be at least 2 components (including this one) to share a communicator\n",
                __func__);
        goto end;
    }

    // Make sure this component is included in the list
    int found = 0;
    // printf("clean comps = %s\n", clean_comps);
    for (int i = 0; i < actual_num_components; i++) {
        if (sorted_components[i] == app->SelfComponent->id) {
            found = 1;
            break;
        }
    }
    if (!found) {
        App_Log(APP_WARNING, "%s: You must include \"self\" (%s, %d) in the list of components"
            " when requesting a shared communicator\n", __func__, App_MPMD_ComponentIdToName(app->SelfComponent->id),
            app->SelfComponent->id);
        goto end;
    }

    // Check whether a communicator has already been created for this set of components
    {
        const TComponentSet* set = find_set(app, sorted_components, actual_num_components);
        if (set) {
            App_Log(APP_DEBUG, "%s: Found already existing set at %p\n", __func__, set);
            shared_comm = set->communicator;
            goto end;
        }
    }

    // Not created yet, so we have to do it now
    const TComponentSet* set = create_set(app, sorted_components, actual_num_components);

    if (set == NULL) {
        // Oh nooo, something went wrong
        App_Log(APP_ERROR, "%s: Unable to create a communicator for the given set (%s)\n", __func__, comps);
    }
    else {
        shared_comm = set->communicator;
    }

end:
    if (shared_comm == MPI_COMM_NULL) {
        App_Log(APP_ERROR, "%s: Communicator is NULL for components %s\n", __func__, comps);
    }

    free(sorted_components);
    return shared_comm;
}

//!> Callable from Fortran: Wrapper that converts the C communicator returned by
//!> App_MPMD_GetSharedComm into a Fortran communicator.
//!> \return The Fortran communicator shared by the given components
MPI_Fint App_MPMD_GetSharedComm_F(
    const int32_t* Components,      //!< List of component IDs that should be grouped
    const int32_t NumComponents    //!< How many components there are in the list
) {
    return MPI_Comm_c2f(App_MPMD_GetSharedComm(Components, NumComponents));
}

//! \return 1 if given component is present in this MPMD context, 0 if not.
int32_t App_MPMD_HasComponent(const char * const ComponentName) {
    // const TApp* app = App_GetInstance();

    // App_Log(APP_DEBUG, "%s: Checking for presence of component %d (%s)\n", __func__, ComponentId, ComponentName);

    // for(int i = 0; i < app->NumComponents; i++) {
        // if (app->AllComponents[i].id == (int)ComponentId) return 1;
    // }

    // App_Log(APP_DEBUG, "%s: Component %d (%s) not found\n", __func__, ComponentId, ComponentName);

    // not found
    return 0;
}

//> Initialize the given component set with specific values
void init_component_set(
    TComponentSet* set,         //!< The set we are initializing
    const int* component_ids,   //!< List of component IDs that are part of the set
    const int num_components,   //!< How many components are in the set
    const MPI_Comm communicator,//!< MPI communicator that is common (and exclusive) to the given components
    const MPI_Group group       //!< MPI group that contains all (global) ranks from the given components
) {
    set->num_components = num_components;
    set->component_ids = (int*)malloc(num_components * sizeof(int));
    memcpy(set->component_ids, component_ids, num_components * sizeof(int));
    set->communicator = communicator;
    set->group = group;
}

//!> Create a set of components within this MPMD context. This will create a communicator for them.
//!> \return A newly-created set (pointer). NULL if there was an error
const TComponentSet* create_set(
    TApp* App,                  //!< TApp instance. *Must be initialized already.*
    //!> List of components IDs in the set. *Must be sorted in ascending order and without duplicates*.
    const int* Components,
    const int NumComponents //!< Number of components in the set
) {
    // Choose (arbitrarily) initial array size to be the total number of components in the context
    if (App->Sets == NULL) {
        App->Sets = (TComponentSet*)malloc(App->NumComponents * sizeof(TComponentSet));
        App->SizeSets = App->NumComponents;
    }

    // Make sure the array is large enough to contain the new set.
    // We need to allocate a new (larger) one if not, and transfer all existing sets to it
    if (App->NbSets >= App->SizeSets) {
        const int old_size = App->SizeSets;
        const int new_size = old_size * 2;

        TComponentSet* new_list = (TComponentSet*)malloc(new_size * sizeof(TComponentSet));
        memcpy(new_list, App->Sets, old_size * sizeof(TComponentSet));
        free(App->Sets);
        App->Sets = new_list;
        App->SizeSets = new_size;
    }

    TComponentSet* new_set = &App->Sets[App->NbSets];
    *new_set = default_set;

    // Find the position of each target component within the list of all components
    int component_pos[NumComponents];
    for (int i = 0, pos = 0; i < NumComponents; i++) {
        for(; App->AllComponents[pos].id < Components[i]; pos++)
            ;
        if (App->AllComponents[pos].id == Components[i]) {
            component_pos[i] = pos;
            pos++;
        }
    }

    // Share the world ranks of other components among all PEs of this component (if not done already)
    for (int i = 0; i < NumComponents; i++) {
        TComponent* comp = &App->AllComponents[component_pos[i]];
        if (!comp->already_shared) {
            if (comp->ranks == NULL) comp->ranks = (int*)malloc(comp->num_pes * sizeof(int));
            MPI_Bcast(comp->ranks, comp->num_pes, MPI_INT, 0, App->SelfComponent->comm);
            comp->already_shared = 1;
        }
    }

    // Create the set
    {
        MPI_Group main_group;
        MPI_Group union_group;
        MPI_Comm  union_comm;

        MPI_Comm_group(App->MainComm, &main_group);

        int group_rank;
        MPI_Group_rank(main_group, &group_rank);

        int total_num_ranks = 0;
        for (int i = 0; i < NumComponents; i++) {
            total_num_ranks += App->AllComponents[component_pos[i]].num_pes;
        }

        int* all_ranks = (int*) malloc(total_num_ranks * sizeof(int));
        int running_num_ranks = 0;
        for (int i = 0; i < NumComponents; i++) {
            const TComponent* comp = &(App->AllComponents[component_pos[i]]);
            memcpy(all_ranks + running_num_ranks, comp->ranks, comp->num_pes * sizeof(int));
            running_num_ranks += comp->num_pes;
        }

        MPI_Group_incl(main_group, total_num_ranks, all_ranks, &union_group);

        // This call is collective among all group members, but not main_comm
        MPI_Comm_create_group(App->MainComm, union_group, 0, &union_comm);

        // Initialize in place, in the list of sets
        init_component_set(new_set, Components, NumComponents, union_comm, union_group);
        App->NbSets++;

        free(all_ranks);
    }

    return new_set;
}

//!> Get the string (name) that corresponds to the given component ID
const char* App_MPMD_ComponentIdToName(const TApp_MPMDId ComponentId) {
    switch (ComponentId) {
        case APP_MPMD_NONE:        return "[none]";
        case APP_MPMD_GEM_ID:      return "GEM";
        case APP_MPMD_IOSERVER_ID: return "IOSERVER";
        case APP_MPMD_IRIS_ID:     return "IRIS";
        case APP_MPMD_NEMO_ID:     return "NEMO";

        case APP_MPMD_TEST1_ID: return "TEST1";
        case APP_MPMD_TEST2_ID: return "TEST2";
        case APP_MPMD_TEST3_ID: return "TEST3";
        case APP_MPMD_TEST4_ID: return "TEST4";
        case APP_MPMD_TEST5_ID: return "TEST5";

        default: return "[unknown component]";
    }
}

//!> Find the component set that corresponds to the given list of IDs within this MPMD context.
//!> \return The set containing the given components (if found), or NULL if it wasn't found.
const TComponentSet* find_set(
    const TApp* App,        //!< The App object where we are looking
    const int* Components,  //!< What components are in the set we want. *Must be sorted in ascending order and without duplicates*.
    const int NumComponents//!< How many components are in the set
) {
    for (int i_set = 0; i_set < App->NbSets; i_set++) {
        const TComponentSet* set = &App->Sets[i_set];
        if (set->num_components == NumComponents) {
            int all_same = 1;
            for (int i_comp = 0; i_comp < NumComponents; i_comp++) {
                if (set->component_ids[i_comp] != Components[i_comp]) {
                    all_same = 0;
                    break;
                }
            }
            if (all_same == 1) return set;
        }
    }

    // Set was not found
    return NULL;
}

void get_comm_string(
    const MPI_Comm comm,
    char* buffer //!< Should be at least 17 characters long
) {
    if (comm == MPI_COMM_NULL) {
        strcpy(buffer, "MPI_COMM_NULL");
    } else {
        sprintf(buffer, "%s", "[something]");
        buffer[16] = '\0';
    }
}

void get_ranks_string(
    const int* ranks,
    const int num_ranks,
    const int with_numbers,
    char* buffer //!< Should be at least 8*15 characters long (120?)
) {
    if (ranks == NULL) {
        strcpy(buffer, "[null]");
    }
    else if (!with_numbers) {
        strcpy(buffer, "[present]");
    }
    else {
        const int MAX_PRINT = 15;
        const int limit = num_ranks < MAX_PRINT ? num_ranks : MAX_PRINT;
        for (int i = 0; i < limit; i++) {
            sprintf(buffer + i * 8, " %7d", ranks[i]);
        }
    }
}

//! Print the content of the given \ref TComponent instance
void print_component(
    const TComponent* comp,   //!< The component whose info we want to print
    const int         verbose //!< Whether to print the global rank of every PE in the component
) {
    char comm_str[32];
    char ranks_str[256];

    get_comm_string(comp->comm, comm_str);
    get_ranks_string(comp->ranks, comp->num_pes, verbose, ranks_str);
    App_Log(APP_DEBUG,
            "Component %s: \n"
            "  id:              %d\n"
            "  comm:            %s\n"
            "  num_pes:         %d\n"
            "  already_shared:  %d\n"
            "  ranks:           %s\n",
            App_MPMD_ComponentIdToName(comp->id), comp->id, comm_str, comp->num_pes, comp->already_shared, ranks_str);
}

void print_set(
    const TComponentSet* set
) {
    char ids_str[1024];
    char comm_str[32];

    get_ranks_string(set->component_ids, set->num_components, 1, ids_str);
    get_comm_string(set->communicator, comm_str);

    App_Log(APP_INFO,
        "Set: \n"
        "  num_components: %d\n"
        "  components: %s\n"
        "  communicator: %s\n", set->num_components, ids_str, comm_str);
}
