
#include "MPMD.h"

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


////////////////
// Prototypes
TApp* get_app_instance(void);
void print_component(const TComponent* comp, const int verbose);
const TComponentSet* find_set(const TApp* app, const int* components, const int num_components);
const TComponentSet* create_set(TApp* app, const int* components, const int num_components);

//! Reset values in TComponent instance
void init_comp(TComponent* comp) {
    comp->id = MPMD_NONE;
    comp->comm = MPI_COMM_NULL;
    comp->num_pes = 0;
    comp->already_shared = 0;
    comp->ranks = NULL;
}

//! Initialize a common MPMD context by telling everyone who we are as a process.
//! This is a collective call. Everyone who participate in it will know who else
//! is on board and will be able to ask for a communicator in common with any other
//! participant (or even multiple other participants at once).
TApp* Mpmd_Init(const TApp_MpmdId component_id) {
    // #pragma omp single // For this entire function
    {
    //////////////////////////////
    // Start by initializing MPI
    const int required = MPI_THREAD_MULTIPLE;
    const MPI_Comm main_comm = MPI_COMM_WORLD;
    
    int provided, world_size;
    MPI_Init_thread(NULL, NULL, required, &provided);

    TApp* app = App_Init(0, component_id_to_name(component_id), "1.0-alpha", "mpmd context attempt", "now");
    App_Start();

    App_Log(APP_DEBUG, "%s: Initializing component %s (ID %d)\n", __func__, component_id_to_name(component_id),
            component_id);
    
    MPI_Comm_size(main_comm, &world_size);
    MPI_Comm_rank(main_comm, &app->world_rank);

    if (provided != required) {
        if (app->world_rank == 0) {
            App_Log(APP_ERROR,
                    "%s: In MPMD context initialization: your system does NOT support MPI_THREAD_MULTIPLE\n", __func__);
        }
        MPI_Finalize();
        exit(-1);
    }

    app->main_comm = main_comm;

    ////////////////////////////////////////
    // Determine which programs are present
    MPI_Comm component_comm;
    MPI_Comm_split(main_comm, component_id, app->world_rank, &component_comm);
    MPI_Comm_rank(component_comm, &app->component_rank);

    // Declare that rank 0 of this component is "active" as a logger
    if (app->component_rank == 0) App_LogRank(app->world_rank);

    App_Log(APP_INFO, "%s: Initializing component %s (ID %d)\n",
            __func__, component_id_to_name(component_id), component_id);

    if (app->world_rank == 0 && app->component_rank != 0) {
        App_Log(APP_FATAL, "%s: Global root should also be the root of its own component\n", __func__);
    }

    ////////////////////////////////////
    // Initialize individual components
    MPI_Comm roots_comm = MPI_COMM_NULL;
    int component_pos = -1;
    if (app->component_rank == 0) {
        // Use component ID as key so that the components are sorted in ascending order
        MPI_Comm_split(main_comm, 0, component_id, &roots_comm);
        MPI_Comm_size(roots_comm, &app->num_components);
        MPI_Comm_rank(roots_comm, &component_pos);

        App_Log(APP_DEBUG, "%s: (%s) There are %d components present\n",
                __func__, component_id_to_name(component_id), app->num_components);
    }
    else {
        MPI_Comm dummy_comm;
        MPI_Comm_split(main_comm, 1, component_id, &dummy_comm);
    }

    ////////////////////////////////////////////////////
    // Transmit basic component information to every PE
    MPI_Bcast(&app->num_components, 1, MPI_INT, 0, main_comm);

    app->all_components = (TComponent*)malloc(app->num_components * sizeof(TComponent));
    for (int i = 0; i < app->num_components; i++) init_comp(&app->all_components[i]);

    // Each component root should have a list of the world rank of every other root
    // (we don't know which one is the WORLD root)
    int root_world_ranks[app->num_components];
    if (app->component_rank == 0) {
        MPI_Allgather(&app->world_rank, 1, MPI_INT, root_world_ranks, 1, MPI_INT, roots_comm);
    }

    // Send the component roots to everyone
    MPI_Bcast(root_world_ranks, app->num_components, MPI_INT, 0, main_comm);
    MPI_Bcast(&component_pos, 1, MPI_INT, 0, component_comm);

    // Select which component belongs to this PE and initialize its struct within this PE
    app->self_component = &app->all_components[component_pos];
    app->self_component->id = component_id;
    MPI_Comm_size(component_comm, &(app->self_component->num_pes));

    // Send basic component information to everyone
    for (int i = 0; i < app->num_components; i++) {
        MPI_Bcast(&app->all_components[i], sizeof(TComponent), MPI_BYTE, root_world_ranks[i], main_comm);
    }

    // Self-component info that is only valid on a PE from this component
    app->self_component->comm  = component_comm;
    app->self_component->ranks = (int*)malloc(app->self_component->num_pes * sizeof(int));

    /////////////////////////////////////////////////////
    // Determine global ranks of the PEs in this component and share with other components

    // Each component gathers the world ranks of its members
    MPI_Allgather(&app->world_rank, 1, MPI_INTEGER, app->self_component->ranks, 1, MPI_INT, component_comm);
    app->self_component->already_shared = 1;

    // Every component root (one by one) sends the ranks of its members to every other component root
    // This way seems easier than to set up everything to be able to use MPI_Allgatherv
    // We only send the list of all other ranks to the roots (rather than aaalllll PEs) to avoid a bit
    // of communication and storage
    if (app->component_rank == 0) {
        for (int i = 0; i < app->num_components; i++) {
            if (app->all_components[i].ranks == NULL)
                app->all_components[i].ranks = (int*)malloc(app->all_components[i].num_pes * sizeof(int));

            MPI_Bcast(app->all_components[i].ranks, app->all_components[i].num_pes, MPI_INT, i, roots_comm);
        }
    }

    // MPI_Finalize(); return NULL;

    // Print some info about the components, for debugging
    if (app->world_rank == 0) {
        App_Log(APP_DEBUG, "%s: Num components = %d\n", __func__, app->num_components);
        for (int i = 0; i < app->num_components; i++) {
            print_component(&app->all_components[i], 0);
        }
    }

    // MPI_Comm_free(&component_comm);

    } // omp single

    // MPI_Finalize();

    return get_app_instance();
}

void Mpmd_Finalize(/* TApp* app */) {
    // return;
    // TODO decide if we call MPI_Finalize in this function 
    // #pragma omp single
    {
        TApp* app = get_app_instance();
        if (app->main_comm != MPI_COMM_NULL) {

            print_component(app->self_component, 1);

            if (app->sets != NULL) {
                for (int i = 0; i < app->num_sets; i++) {
                    free(app->sets[i].component_ids);
                    // MPI_Comm_free(&(app->sets[i].communicator));
                    // MPI_Group_free(&(app->sets[i].group));
                }
                free(app->sets);
                app->sets = NULL;
            }
            app->num_sets = 0;

            for (int i = 0; i < app->num_components; i++) {
                if (app->all_components[i].ranks != NULL) {
                    App_Log(APP_DEBUG, "Freeing ranks %p\n", app->all_components[i].ranks);
                    free(app->all_components[i].ranks);
                }
            }
            // MPI_Comm_free(&(app->self_component->comm));
            free(app->all_components);
            app->all_components = NULL;
            app->self_component = NULL;
            app->num_components = 0;

            MPI_Finalize();
            app->main_comm = MPI_COMM_NULL;
        }
    } // omp single
}

int contains(const int* list, const int num_items, const int item) {
    for (int i = 0; i < num_items; i++) if (list[i] == item) return 1;
    return 0;
}


void print_list(int* list, int size) {
    char list_str[512];
    for (int i = 0; i < size; i++) sprintf(list_str + 8*i, "%7d ", list[i]);
    App_Log(APP_EXTRA, "List: %s\n", list_str);
}

//!> From the given list of components, get the same list in ascending order and without any duplicate.
int* get_clean_component_list(const int* components, const int num_components, int* actual_num_components) {

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
MPI_Comm Mpmd_Get_own_comm(void) {
    TApp* app = get_app_instance();
    return app->self_component->comm;
}

//! \return The (Fortran) communicator for all PEs part of the same component as me.
MPI_Fint Mpmd_Get_own_comm_f(void) {
    return MPI_Comm_c2f(Mpmd_Get_own_comm());
}

//!> Retrieve a communicator that encompasses all PEs part of the components
//!> in the given list. If the communicator does not already exist, it will be created.
//!> _This function call is collective if and only if the communicator gets created._
MPI_Comm Mpmd_Get_shared_comm(
    //!> The list of components IDs for which we want a shared communicator.
    //!> This list *must* contain the component of the calling PE. It may contain
    //!> duplicate IDs and does not have to be in a specific order.
    const int32_t* components,
    const int32_t num_components
) {
    MPI_Comm shared_comm = MPI_COMM_NULL;

    TApp* app = get_app_instance();

    // Sanitize the list of components
    int actual_num_components = 0;
    int* sorted_components = get_clean_component_list(components, num_components, &actual_num_components);

    char comps[256] = {0};
    char clean_comps[256] = {0};
    for (int i = 0; i < 10 && i < num_components; i++) sprintf(comps + 8*i, " %7d", components[i]);
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
        if (sorted_components[i] == app->self_component->id) {
            found = 1;
            break;
        }
    }
    if (!found) {
        App_Log(APP_WARNING, "%s: You must include \"self\" (%s, %d) in the list of components"
            " when requesting a shared communicator\n", __func__, component_id_to_name(app->self_component->id),
            app->self_component->id);
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

MPI_Fint Mpmd_Get_shared_comm_f(
    const int32_t* components,
    const int32_t num_components
) {
    return MPI_Comm_c2f(Mpmd_Get_shared_comm(components, num_components));
}

//! \return 1 if given component is present in this MPMD context, 0 if not.
int32_t Mpmd_has_component(const TApp_MpmdId component_id) {
    const TApp* app = get_app_instance();

    App_Log(APP_DEBUG, "%s: Checking for presence of component %d (%s)\n",
            __func__, component_id, component_id_to_name(component_id));

    for(int i = 0; i < app->num_components; i++) {
        if (app->all_components[i].id == (int)component_id) return 1;
    }

    App_Log(APP_DEBUG, "%s: Component %d (%s) not found\n",
            __func__, component_id, component_id_to_name(component_id));

    // not found
    return 0;
}

MPI_Group merge_groups(const MPI_Group* groups, const int num_groups) {
    if (num_groups < 1) return MPI_GROUP_NULL;
    if (num_groups == 1) return groups[0];

    App_Log(APP_DEBUG, "%s: Merging %d groups\n", __func__, num_groups);

    MPI_Group unions[num_groups];
    unions[0] = groups[0];
    for (int i = 1; i < num_groups; i++) {
        MPI_Group_union(groups[i], unions[i - 1], &unions[i]);
    }
    
    return unions[num_groups - 1];
}

void init_component_set(
    TComponentSet* set,
    const int* component_ids,
    const int num_components,
    const MPI_Comm communicator,
    const MPI_Group group
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
    TApp* app,                  //!< TApp instance. *Must be initialized already.*
    //!> List of components IDs in the set. *Must be sorted in ascending order and without duplicates*.
    const int* components,
    const int num_components //!< Number of components in the set
) {
    // Choose (arbitrarily) initial array size to be the total number of components in the context
    if (app->sets == NULL) {
        app->sets = (TComponentSet*)malloc(app->num_components * sizeof(TComponentSet));
        app->sets_size = app->num_components;
    }

    // Make sure the array is large enough to contain the new set.
    // We need to allocate a new (larger) one if not, and transfer all existing sets to it
    if (app->num_sets >= app->sets_size) {
        const int old_size = app->sets_size;
        const int new_size = old_size * 2;

        TComponentSet* new_list = (TComponentSet*)malloc(new_size * sizeof(TComponentSet));
        memcpy(new_list, app->sets, old_size * sizeof(TComponentSet));
        free(app->sets);
        app->sets = new_list;
    }

    TComponentSet* new_set = &app->sets[app->num_sets];

    // Find the position of each target component within the list of all components
    int component_pos[num_components];
    for (int i = 0, pos = 0; i < num_components; i++) {
        for(; app->all_components[pos].id < components[i]; pos++)
            ;
        if (app->all_components[pos].id == components[i]) {
            component_pos[i] = pos;
            pos++;
        }
    }

    // Share the world ranks of other components among all PEs of this component (if not done already)
    for (int i = 0; i < num_components; i++) {
        TComponent* comp = &app->all_components[component_pos[i]];
        if (!comp->already_shared) {
            if (comp->ranks == NULL) comp->ranks = (int*)malloc(comp->num_pes * sizeof(int));
            MPI_Bcast(comp->ranks, comp->num_pes, MPI_INT, 0, app->self_component->comm);
            comp->already_shared = 1;
        }
    }

    // Create the set
    {
        MPI_Group main_group;
        MPI_Group subgroups[num_components];
        MPI_Group union_group;
        MPI_Comm  union_comm;

        MPI_Comm_group(app->main_comm, &main_group);

        int group_rank;
        MPI_Group_rank(main_group, &group_rank);

        int total_num_ranks = 0;
        for (int i = 0; i < num_components; i++) total_num_ranks += app->all_components[component_pos[i]].num_pes;

        int* all_ranks = (int*) malloc(total_num_ranks * sizeof(int));
        int running_num_ranks = 0;
        for (int i = 0; i < num_components; i++) {
            const TComponent* comp = &(app->all_components[component_pos[i]]);

            // char ranks_str[512];
            // for (int j = 0; j < comp->num_pes; j++) {
            //     sprintf(ranks_str + 8*j, "%7d ", comp->ranks[j]);
            // }
            // App_Log(APP_DEBUG, "%s: (%s) creating subgroup from %s with ranks %s\n",
            //         __func__, component_id_to_name(app->self_component->id), component_id_to_name(comp->id), ranks_str);

            memcpy(all_ranks + running_num_ranks, comp->ranks, comp->num_pes * sizeof(int));
            running_num_ranks += comp->num_pes;

            // MPI_Group_incl(main_group, comp->num_pes, comp->ranks, &subgroups[i]);
        }

        // Get the union of all these groups. Using a function to avoid overwriting result during the process
        // union_group = merge_groups(subgroups, num_components);
        
        MPI_Barrier(app->self_component->comm);
        char ranks_str[10000];
        for (int j = 0; j < total_num_ranks; j++) sprintf(ranks_str + j*8, "%7d ", all_ranks[j]);
        App_Log(APP_DEBUG, "%s: (%s, rank %d, group rank %d) creating group with ranks %s\n",
        // fprintf(stderr, "%s: (%s, rank %d, group rank %d) creating group with ranks %s\n",
                __func__, component_id_to_name(app->self_component->id), app->world_rank, group_rank, ranks_str);

        const int status = MPI_Group_incl(main_group, total_num_ranks, all_ranks, &union_group);
        // fprintf(stderr, "Group created for rank %d, status %s\n",
        //         app->world_rank, (status == MPI_SUCCESS ? "MPI_SUCCESS" : "ERROR"));

        // This call is collective among all group members, but not main_comm
        {
            // App_Log(APP_DEBUG, "%s: (%s) Creating shared comm\n",
            // fprintf(stderr, "%s: (%s) Creating shared comm\n",
            //         __func__, component_id_to_name(app->self_component->id));
        }
        MPI_Comm_create_group(app->main_comm, union_group, 0, &union_comm);

        // Initialize in place, in the list of sets
        init_component_set(new_set, components, num_components, union_comm, union_group);
        app->num_sets++;

        // for (int i = 0; i < num_components; i++) MPI_Group_free(&subgroups[i]);
        // MPI_Group_free(&union_group);

        free(all_ranks);
    }


    return new_set;
}

const char* component_id_to_name(const TApp_MpmdId component_id) {
    switch (component_id)
    {
    case MPMD_NONE:            return "[none]";
    case MPMD_GEM_ID:          return "GEM";
    case MPMD_GEM_IOSERVER_ID: return "GEM_IOSERVER";
    case MPMD_IRIS_ID:         return "IRIS";
    case MPMD_NEMO_ID:         return "NEMO";

    case MPMD_TEST1_ID: return "TEST1";
    case MPMD_TEST2_ID: return "TEST2";
    case MPMD_TEST3_ID: return "TEST3";
    case MPMD_TEST4_ID: return "TEST4";
    case MPMD_TEST5_ID: return "TEST5";

    default: return "[unknown component]";
    }
}

//!> Find the component set that corresponds to the given list of IDs within this MPMD context.
//!> \return The set containing the given components (if found), or NULL if it wasn't found.
const TComponentSet* find_set(
    const TApp* app,        //!< The App object where we are looking
    const int* components,  //!< What components are in the set we want. *Must be sorted in ascending order and without duplicates*.
    const int num_components//!< How many components are in the set
) {
    for (int i_set = 0; i_set < app->num_sets; i_set++) {
        const TComponentSet* set = &app->sets[i_set];
        if (set->num_components == num_components) {
            int all_same = 1;
            for (int i_comp = 0; i_comp < num_components; i_comp++) {
                if (set->component_ids[i_comp] != components[i_comp]) {
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
    }
    else {
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
            component_id_to_name(comp->id), comp->id, comm_str, comp->num_pes, comp->already_shared, ranks_str);
}
