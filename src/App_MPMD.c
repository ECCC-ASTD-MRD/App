//! \file
//! Implementation of MPMD functions

#include "App_MPMD.h"

#include <stdlib.h>
#include <string.h>


//! MPMD component identification
typedef struct {
    //! ID of this component (0 is reserved for "none" component)
    int id;
    //! Name of the component
    char name[APP_MAX_COMPONENT_NAME_LEN];
 } TComponentId;

//! Default values for a TComponentSet struct
static const TComponentSet defaultSet = {
    .nbComponents = 0,
    .componentIds = NULL,
    .comm = MPI_COMM_NULL,
    .group = MPI_GROUP_NULL
};


// Prototypes
static void printComponent(const TComponent * const comp, const int verbose);
static void printSet(const TComponentSet * const set);
static TComponentSet * findSet(const TApp * const app, const int * const components, const int nbComponents);
static TComponentSet * createSet(TApp * const app, const int * const components, const int nbComponents);


//! Find unique components
//! \return Pointer to a new sorted array of unique components.
//! The index of the array corresponds to the component id/MPI_APPNUM and can therefore be addressed directly.
//! The caller must free when no longer needed.
static TComponent * findUniqueComponents(
    const int worldSize,
    const TComponentId * const peComponentIds,
    int * const nbComponents
) {
    *nbComponents = 0;
    for (int i = 0; i < worldSize; i++) {
        if (peComponentIds[i].id > *nbComponents) {
            *nbComponents = peComponentIds[i].id;
        }
    }
    // The number of components is equal to the highest id + 1
    (*nbComponents)++;
    TComponent * const uniqueComponents = (TComponent *)calloc(*nbComponents, sizeof(TComponent));
    for (int compIdx = 0; compIdx < *nbComponents; compIdx++) {
        for (int i = 0; i < worldSize; i++) {
            if (peComponentIds[i].id == compIdx) {
                uniqueComponents[compIdx].id = peComponentIds[i].id;
                strncpy(uniqueComponents[compIdx].name, peComponentIds[i].name, APP_MAX_COMPONENT_NAME_LEN);
                break;
            }
        }
    }

    for (int compIdx = 0; compIdx < *nbComponents; compIdx++) {
        App_Log(APP_DEBUG, "%s: compIdx = %d, id = %d, name = %s\n", __func__, compIdx, uniqueComponents[compIdx].id, uniqueComponents[compIdx].name);
    }

    // Verify that we don't have duplicate names
    for (int i = 0; i < *nbComponents; i++) {
        for (int j = 0; j < *nbComponents; j++) {
            // It's not duplicated if it's the component itself
            if (i != j) {
                if (strncmp(uniqueComponents[i].name, uniqueComponents[j].name, APP_MAX_COMPONENT_NAME_LEN) == 0) {
                    App_Log(APP_FATAL, "%s: Duplicate component name detected (%s)!\n", __func__, uniqueComponents[j].name);

                }
            }

        }
    }

    App_Log(APP_DEBUG, "%s: *nbComponents = %d\n", __func__, *nbComponents);
    return uniqueComponents;
}


//! Get the component id corresponding to the provided name
int App_MPMD_GetComponentId(
    //! [in] Component name
    const char * const componentName
) {
    //! \note Ids correspond to the MPI_APPNUM
    //! \return Component id if found, -1 otherwise

    const TApp * const app = App_GetInstance();
    if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);

    for (int i = 0; i < app->NumComponents; i++) {
        if (strncmp(app->AllComponents[i].name, componentName, APP_MAX_COMPONENT_NAME_LEN) == 0) {
            return app->AllComponents[i].id;
        }
    }
    return -1;
}


//! Get the id of the component to which this PE belongs
int App_MPMD_GetSelfComponentId() {
    //! \return Component id of this PE

    const TApp * const app = App_GetInstance();
    if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);

    return app->SelfComponent->id;
}


//! Initialize a common MPMD context by telling everyone who we are as a process.
//! This is a collective call. Everyone who participate in it will know who else
//! is on board and will be able to ask for a communicator in common with any other
//! participant (or even multiple other participants at once).
TApp * App_MPMD_Init(
    //! [in] Name of this component. Must be APP_MAX_COMPONENT_NAME_LEN - 1 characters at most
    const char * const componentName,
    //! [in] Version of this application
    const char * const version
) {

    // #pragma omp single // For this entire function
    // {
    //     int provided;
    //     MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    //     int world_rank;
    //     int world_size;
    //     MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    //     MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    //     printf("%s: PE %04d/%04d, component %s\n", __func__, world_rank, world_size, componentName);
    //     fflush(NULL);
    //     MPI_Barrier(MPI_COMM_WORLD);
    //     MPI_Finalize();
    //     exit(0);
    // }

    #pragma omp single // For this entire function
    {
        // Start by initializing MPI

        int provided;
        //! \todo Figure out if all programs or even threads need to have the same value for required
        MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);

        TApp * const app = App_Init(0, componentName, version, "mpmd context attempt", "now");
        App_Start();

        //! \todo Remove this once we have figured out how this mechanism is supposed to work
        App_ToleranceLevel("ERROR");

        if (provided != MPI_THREAD_MULTIPLE) {
            if (app->WorldRank == 0) {
                App_Log(APP_ERROR, "%s: In MPMD context initialization: your system does NOT support MPI_THREAD_MULTIPLE\n", __func__);
            }
            MPI_Finalize();
            exit(-1);
        }

        int world_size;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &app->WorldRank);

        printf("%s: PE %d/%d, component %s\n", __func__, app->WorldRank, world_size, componentName);
        fflush(NULL);
        MPI_Barrier(MPI_COMM_WORLD);

        App_Log(APP_DEBUG, "%s: Initializing component %s PE %04d/%04d\n", __func__, componentName, app->WorldRank, world_size);

        app->MainComm = MPI_COMM_WORLD;

        // We need to assign a unique id for the component, but multiple mpi processors may share the same component name
        // This needs to be a collective call : MPI_Gather()
        // In order to allocate our input buffer, we must set a maximum component name length
        TComponentId * peComponentIds = NULL;
        if (app->WorldRank == 0) {
            peComponentIds = malloc(world_size * sizeof(TComponentId));
        }

        int * appNum;
        {
            int flag;
            MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_APPNUM, &appNum, &flag);
        }
        TComponentId peComponentId = {.id = *appNum};
        strncpy(peComponentId.name, componentName, APP_MAX_COMPONENT_NAME_LEN);
        MPI_Gather(&peComponentId, sizeof(TComponentId), MPI_BYTE, peComponentIds, sizeof(TComponentId), MPI_BYTE, 0, MPI_COMM_WORLD);
        if (app->WorldRank == 0) {
            app->AllComponents = findUniqueComponents(world_size, peComponentIds, &(app->NumComponents));
            App_Log(APP_DEBUG, "%s: app->NumComponents = %d\n", __func__, app->NumComponents);
            free(peComponentIds);
        }
        MPI_Bcast(&(app->NumComponents), 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (app->WorldRank != 0) {
            app->AllComponents = malloc(app->NumComponents * sizeof(TComponent));
        }
        MPI_Bcast(app->AllComponents, app->NumComponents * sizeof(TComponent), MPI_BYTE, 0, MPI_COMM_WORLD);
        // At this point all processes have the same list of unique component id-names pairs

        // printf("%s: PE %d/%d, component %s\n", __func__, app->WorldRank, world_size, componentName);
        // fflush(NULL);
        // MPI_Barrier(MPI_COMM_WORLD);
        // MPI_Finalize();
        // exit(-1);

        const int componentId = *appNum;

        app->SelfComponent = &app->AllComponents[componentId];
        app->SelfComponent->id = componentId;
        MPI_Comm_split(MPI_COMM_WORLD, componentId, app->WorldRank, &app->SelfComponent->comm);
        MPI_Comm_rank(app->SelfComponent->comm, &app->ComponentRank);
        MPI_Comm_size(app->SelfComponent->comm, &(app->SelfComponent->nbPes));

        // Declare that rank 0 of this component is "active" as a logger
        if (app->ComponentRank == 0) App_LogRank(app->WorldRank);

        App_Log(APP_INFO, "%s: Initializing component %s (ID %d) with %d PEs\n", __func__, componentName, componentId, app->SelfComponent->nbPes);

        if (app->WorldRank == 0 && app->ComponentRank != 0) {
            App_Log(APP_FATAL, "%s: Global root should also be the root of its own component\n", __func__);
        }

        // printf("%s: PE %d/%d, component %d/%d\n", __func__, app->WorldRank, world_size, app->ComponentRank, app->SelfComponent->nbPes);
        // fflush(NULL);
        // MPI_Barrier(MPI_COMM_WORLD);
        // MPI_Finalize();
        // exit(-1);

        // Create a communicator with the roots of each component
        MPI_Comm rootsComm = MPI_COMM_NULL;
        int rootRank = -1;
        int rootSize = -1;
        {
            const int isComponentRoot = app->ComponentRank == 0 ? 0 : MPI_UNDEFINED;
            // Use component ID as key so that the components are sorted in ascending order
            MPI_Comm_split(MPI_COMM_WORLD, isComponentRoot, componentId, &rootsComm);
            // The PEs that are not to roots get the MPI_COMM_NULL communicator which causes most MPI functions to fail
            if (rootsComm != MPI_COMM_NULL) {
                MPI_Comm_rank(rootsComm, &rootRank);
            }
            MPI_Comm_size(rootsComm, &rootSize);
        }
        App_Log(APP_DEBUG, "%s: rootsComm = %d, rootRank = %d\n", __func__, rootsComm, rootRank);

        printf("%s: PE %d/%d, component %d/%d rootSize = %d\n", __func__, 
            app->WorldRank, world_size, app->ComponentRank, app->SelfComponent->nbPes, rootSize);
        printf("%s: PE %d/%d, component %d/%d\n", __func__, app->WorldRank, world_size, app->ComponentRank, app->SelfComponent->nbPes);
        fflush(NULL);
        MPI_Barrier(MPI_COMM_WORLD);

        // Each component root should have a list of the world rank of every other root
        // (we don't know which one is the WORLD root)
        int rootWorldRanks[app->NumComponents];
        if (app->ComponentRank == 0) {
            MPI_Allgather(&app->WorldRank, 1, MPI_INT, rootWorldRanks, 1, MPI_INT, rootsComm);
        }

        printf("%s: PE %d/%d, component %d/%d\n", __func__, app->WorldRank, world_size, app->ComponentRank, app->SelfComponent->nbPes);
        fflush(NULL);

        // Send the component roots to everyone
        MPI_Bcast(&rootWorldRanks, app->NumComponents, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&rootRank, 1, MPI_INT, 0, app->SelfComponent->comm);

        // Send basic component information to everyone
        for (int i = 0; i < app->NumComponents; i++) {
            MPI_Bcast(&app->AllComponents[i], sizeof(TComponent), MPI_BYTE, rootWorldRanks[i], MPI_COMM_WORLD);
        }

        // Self-component info that is only valid on a PE from this component
        app->SelfComponent->ranks = malloc(app->SelfComponent->nbPes * sizeof(int));

        // Determine global ranks of the PEs in this component and share with other components

        // Each component gathers the world ranks of its members
        int retVal = MPI_Allgather(&app->WorldRank, 1, MPI_INT, app->SelfComponent->ranks, 1, MPI_INT, app->SelfComponent->comm);
        App_Log(APP_DEBUG, "%s: MPI_Allgather = %d\n", __func__, retVal);
        app->SelfComponent->shared = 1;


        // Every component root (one by one) sends the ranks of its members to every other component root
        // This way seems easier than to set up everything to be able to use MPI_Allgatherv
        // We only send the list of all other ranks to the roots (rather than aaalllll PEs) to avoid a bit
        // of communication and storage
        if (app->ComponentRank == 0) {
            for (int i = 0; i < app->NumComponents; i++) {
                if (app->AllComponents[i].ranks == NULL) {
                    app->AllComponents[i].ranks = malloc(app->AllComponents[i].nbPes * sizeof(int));
                }

                MPI_Bcast(app->AllComponents[i].ranks, app->AllComponents[i].nbPes, MPI_INT, i, rootsComm);
            }
        }

        // Print some info about the components, for debugging
        if (app->WorldRank == 0) {
            App_Log(APP_DEBUG, "%s: Num components = %d\n", __func__, app->NumComponents);
            for (int i = 0; i < app->NumComponents; i++) {
                printComponent(&app->AllComponents[i], 0);
            }
        }
    } // omp single

    return App_GetInstance();
}


//! Terminate cleanly the MPMD run.
//! Frees the memory allocated by TComponent and TComponentSet structs, and it calls MPI_Finalize()
void App_MPMD_Finalize() {
    #pragma omp single
    {
        TApp * const app = App_GetInstance();
        if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);
        if (app->MainComm != MPI_COMM_NULL) {

            printComponent(app->SelfComponent, 1);

            if (app->Sets != NULL) {
                for (int i = 0; i < app->NbSets; i++) {
                    free(app->Sets[i].componentIds);
                    MPI_Comm_free(&(app->Sets[i].comm));
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


//! Determine if a list of integer contains a given item
int contains(
    //! List in which to search
    const int * const list,
    //! Number of items in the list
    const int nbItems,
    //! Item to search for
    const int item
) {
    //! \return 1 if a certain item is in the given list, 0 otherwise
    for (int i = 0; i < nbItems; i++) if (list[i] == item) return 1;
    return 0;
}


static void printList(
    const int * const list,
    const int size
) {
    if (App_LogLevel(NULL) >= APP_EXTRA) {
        char str[512];
        for (int i = 0; i < size; i++) sprintf(str + 8 * i, "%7d ", list[i]);
        App_Log(APP_EXTRA, "List: %s\n", str);
    }
}


//! Get a sorted list of components without duplication
//! \return Cleaned up list. Caller must free this list when no longer needed.
static int * cleanComponentList(
    const int * const components,      //!< Initial list of component IDs
    const int nbComponents,   //!< How many components there are in the initial list
    int * const nbUniqueComponents  //!< [out] How many components there are in the cleaned-up list
) {

    int tmpList[nbComponents + 1];

    // Perform an insertion sort, while checking for duplicates
    for (int i = 0; i < nbComponents + 1; i++) tmpList[i] = -1;
    tmpList[0] = components[0];
    *nbUniqueComponents = 1;
    App_Log(APP_EXTRA, "nbComponents = %d\n", nbComponents);
    printList(tmpList, nbComponents + 1);
    for (int i = 1; i < nbComponents; i++) {
        const int item = components[i];

        App_Log(APP_EXTRA, "i = %d, *nbUniqueComponents = %d, item = %d\n", i, *nbUniqueComponents, item);
        printList(tmpList, nbComponents + 1);
        App_Log(APP_EXTRA, "duplicate? %d\n", contains(tmpList, *nbUniqueComponents, item));

        if (!contains(tmpList, *nbUniqueComponents, item)) {
            // Loop through sorted list (so far)
            for (int j = i - 1; j >= 0; j--) {
                App_Log(APP_EXTRA, "j = %d\n", j);
                if (tmpList[j] > item) {
                    // Not the right position yet
                    tmpList[j + 1] = tmpList[j];
                    App_Log(APP_EXTRA, "Changed list to... \n");
                    printList(tmpList, nbComponents + 1);

                    if (j == 0) {
                        // Last one, so we know the 1st slot is the right one
                        tmpList[j] = item;
                        (*nbUniqueComponents)++;

                        App_Log(APP_EXTRA, "Changed list again to... \n");
                        printList(tmpList, nbComponents + 1);
                    }
                } else if (tmpList[j] < item) {
                    // Found the right position
                    tmpList[j + 1] = item;
                    (*nbUniqueComponents)++;

                    App_Log(APP_EXTRA, "Correct pos, changed list to... \n");
                    printList(tmpList, nbComponents + 1);
                    break;
                }
            }
        }
    }

    // Put in list with the right size
    int * const cleanList = (int * const)malloc(*nbUniqueComponents * sizeof(int));
    memcpy(cleanList, tmpList, *nbUniqueComponents * sizeof(int));

    return cleanList;
}


//! Get the communicator for for component to which this PE belongs
//! \return The communicator for all PEs part of the same component as me.
MPI_Comm App_MPMD_GetSelfComm() {
    const TApp * const app = App_GetInstance();
    if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);
    return app->SelfComponent->comm;
}


//! \return The (Fortran) communicator for all PEs part of the same component as me.
MPI_Fint App_MPMD_GetSelfComm_F() {
    return MPI_Comm_c2f(App_MPMD_GetSelfComm());
}


//! Retrieve a communicator that encompasses all PEs part of the components
//! in the given list. If the communicator does not already exist, it will be created.
//! _This function call is collective if and only if the communicator gets created._
MPI_Comm App_MPMD_GetSharedComm(
    //!> The list of components IDs for which we want a shared communicator.
    //!> This list *must* contain the component of the calling PE. It may contain
    //!> duplicate IDs and does not have to be in a specific order.
    const int32_t * const components,
    const int32_t nbComponents
) {
    MPI_Comm sharedComm = MPI_COMM_NULL;

    TApp * const app = App_GetInstance();
    if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);

    // Sanitize the list of components
    int nbUniqueComponents = 0;
    int * const uniqueComponents = cleanComponentList(components, nbComponents, &nbUniqueComponents);

    char comps[256] = {0};
    char cleanComps[256] = {0};
    for (int i = 0; i < 10 && i < nbComponents; i++) sprintf(comps + 8*i, " %7d", components[i]);
    for (int i = 0; i < 10 && i < nbUniqueComponents; i++) sprintf(cleanComps + 8*i, " %7d", uniqueComponents[i]);
    App_Log(APP_DEBUG, "%s: Retrieving shared comm for components %s (%s)\n", __func__, comps, cleanComps);

    // Make sure there are enough components in the list
    if (nbUniqueComponents < 2) {
        App_Log(APP_ERROR, "%s: There need to be at least 2 components (including this one) to share a communicator\n",
                __func__);
        goto end;
    }

    // Make sure this component is included in the list
    int found = 0;
    // printf("clean comps = %s\n", cleanComps);
    for (int i = 0; i < nbUniqueComponents; i++) {
        if (uniqueComponents[i] == app->SelfComponent->id) {
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
        const TComponentSet* set = findSet(app, uniqueComponents, nbUniqueComponents);
        if (set) {
            App_Log(APP_DEBUG, "%s: Found already existing set at %p\n", __func__, set);
            sharedComm = set->comm;
            goto end;
        }
    }

    // Not created yet, so we have to do it now
    const TComponentSet * const set = createSet(app, uniqueComponents, nbUniqueComponents);

    if (set == NULL) {
        // Oh nooo, something went wrong
        App_Log(APP_ERROR, "%s: Unable to create a communicator for the given set (%s)\n", __func__, comps);
    } else {
        sharedComm = set->comm;
    }

end:
    if (sharedComm == MPI_COMM_NULL) {
        App_Log(APP_ERROR, "%s: Communicator is NULL for components %s\n", __func__, comps);
    }

    free(uniqueComponents);
    return sharedComm;
}


//! Callable from Fortran: Wrapper that converts the C communicator returned by
//! App_MPMD_GetSharedComm into a Fortran communicator.
//! \return The Fortran communicator shared by the given components
MPI_Fint App_MPMD_GetSharedComm_F(
    //! List of component IDs that should be grouped
    const int32_t * const components,
    //! Number of components
    const int32_t nbComponents
) {
    return MPI_Comm_c2f(App_MPMD_GetSharedComm(components, nbComponents));
}


//! Test if the provided component is present in this MPMD context
//! \return 1 if given component is present in this MPMD context, 0 if not.
int32_t App_MPMD_HasComponent(const char * const componentName) {
    const TApp * const app = App_GetInstance();
    if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);

    App_Log(APP_DEBUG, "%s: Checking for presence of component \"%s\" ...\n", __func__, componentName);

    return App_MPMD_GetComponentId(componentName) >= 0;

    App_Log(APP_DEBUG, "%s: Component \"%s\" not found\n", __func__, componentName);

    // not found
    return 0;
}


//! Initialize the given component set with specific values
static void initComponentSet(
    //! Set to be initialized
    TComponentSet * const set,
    //! List of component IDs that are part of the set
    const int * const componentIds,
    //! Number of components in the set
    const int nbComponents,
    //! MPI communicator that is common (and exclusive) to the given components
    const MPI_Comm comm,
    //! MPI group that contains all (global) PEs from the given components
    const MPI_Group group
) {
    set->nbComponents = nbComponents;
    set->componentIds = (int*)malloc(nbComponents * sizeof(int));
    memcpy(set->componentIds, componentIds, nbComponents * sizeof(int));
    set->comm = comm;
    set->group = group;
}


//! Create a set of components within this MPMD context.
//! This will create a comm for them.
//! \return A newly-created set (pointer). NULL if there was an error
static TComponentSet * createSet(
    //! TApp instance. *Must already be initialized.*
    TApp * const app,
    //! List of components IDs in the set. *Must be sorted in ascending order and without duplicates*.
    const int * const components,
    //! Number of components in the set
    const int nbComponents
) {
    // Choose (arbitrarily) initial array size to be the total number of components in the context
    if (app->Sets == NULL) {
        app->Sets = (TComponentSet*)malloc(app->NumComponents * sizeof(TComponentSet));
        app->SizeSets = app->NumComponents;
    }

    // Make sure the array is large enough to contain the new set.
    // We need to allocate a new (larger) one if not, and transfer all existing sets to it
    if (app->NbSets >= app->SizeSets) {
        const int oldSize = app->SizeSets;
        const int newSize = oldSize * 2;

        TComponentSet * const newSet = (TComponentSet*)malloc(newSize * sizeof(TComponentSet));
        memcpy(newSet, app->Sets, oldSize * sizeof(TComponentSet));
        free(app->Sets);
        app->Sets = newSet;
        app->SizeSets = newSize;
    }

    TComponentSet* newSet = &app->Sets[app->NbSets];
    *newSet = defaultSet;

    // Find the position of each target component within the list of all components
    int componentPos[nbComponents];
    for (int i = 0, pos = 0; i < nbComponents; i++) {
        for(; app->AllComponents[pos].id < components[i]; pos++)
            ;
        if (app->AllComponents[pos].id == components[i]) {
            componentPos[i] = pos;
            pos++;
        }
    }

    // Share the world ranks of other components among all PEs of this component (if not done already)
    for (int i = 0; i < nbComponents; i++) {
        TComponent* comp = &app->AllComponents[componentPos[i]];
        if (!comp->shared) {
            if (comp->ranks == NULL) comp->ranks = (int*)malloc(comp->nbPes * sizeof(int));
            MPI_Bcast(comp->ranks, comp->nbPes, MPI_INT, 0, app->SelfComponent->comm);
            comp->shared = 1;
        }
    }

    // Create the set
    {
        MPI_Group mainGroup;
        MPI_Group unionGroup;
        MPI_Comm  unionComm;

        MPI_Comm_group(app->MainComm, &mainGroup);

        int nbTotalPes = 0;
        for (int i = 0; i < nbComponents; i++) {
            nbTotalPes += app->AllComponents[componentPos[i]].nbPes;
        }

        int * const allRanks = (int*) malloc(nbTotalPes * sizeof(int));
        int currentPe = 0;
        for (int i = 0; i < nbComponents; i++) {
            const TComponent* comp = &(app->AllComponents[componentPos[i]]);
            memcpy(allRanks + currentPe, comp->ranks, comp->nbPes * sizeof(int));
            currentPe += comp->nbPes;
        }

        MPI_Group_incl(mainGroup, nbTotalPes, allRanks, &unionGroup);

        // This call is collective among all group members, but not main_comm
        MPI_Comm_create_group(app->MainComm, unionGroup, 0, &unionComm);

        // Initialize in place, in the list of sets
        initComponentSet(newSet, components, nbComponents, unionComm, unionGroup);
        app->NbSets++;

        free(allRanks);
    }

    return newSet;
}


//! Get the string (name) that corresponds to the given component ID
const char * App_MPMD_ComponentIdToName(const int componentId) {
    //! \todo Implement this function
    const TApp * const app = App_GetInstance();
    if (!app) App_Log(APP_FATAL, "%s: Failed to get app instance!\n", __func__);

    if (0 <= componentId && componentId < app->NumComponents) {
        return app->AllComponents[componentId].name;
    } else {
        return NULL;
    }

}


//! Find the component set that corresponds to the given list of IDs within this MPMD context.
//! \return The set containing the given components (if found), or NULL if it wasn't found.
static TComponentSet * findSet(
    //! TApp instance in which we want to search
    const TApp * app,
    //! What components are in the set we want. *Must be sorted in ascending order and without duplicates*.
    const int * const components,
    //! Number of components in the provided set
    const int nbComponents
) {
    for (int setIdx = 0; setIdx < app->NbSets; setIdx++) {
        TComponentSet * const set = &app->Sets[setIdx];
        if (set->nbComponents == nbComponents) {
            int allSame = 1;
            for (int compIdx = 0; compIdx < nbComponents; compIdx++) {
                if (set->componentIds[compIdx] != components[compIdx]) {
                    allSame = 0;
                    break;
                }
            }
            if (allSame == 1) return set;
        }
    }

    // Set was not found
    return NULL;
}


void getCommStr(
    const MPI_Comm comm,
    //! Output buffer. Must be at least 17 characters long
    char * const buffer
) {
    if (comm == MPI_COMM_NULL) {
        strcpy(buffer, "MPI_COMM_NULL");
    } else {
        sprintf(buffer, "%s", "[something]");
        buffer[16] = '\0';
    }
}


static void getRanksStr(
    const int * const ranks,
    const int nbRanks,
    const int withNumbers,
    //! Output buffer. Should be at least 8*15 characters long (120?)
    char * const buffer
) {
    if (ranks == NULL) {
        strcpy(buffer, "[null]");
    }
    else if (!withNumbers) {
        strcpy(buffer, "[present]");
    } else {
        const int MAX_PRINT = 15;
        const int limit = nbRanks < MAX_PRINT ? nbRanks : MAX_PRINT;
        for (int i = 0; i < limit; i++) {
            sprintf(buffer + i * 8, " %7d", ranks[i]);
        }
    }
}


//! Print the content of the given \ref TComponent instance
static void printComponent(
    //! The component whose info we want to print
    const TComponent * const comp,
    //! Whether to print the global rank of every PE in the component
    const int verbose
) {
    char commStr[32];
    char ranksStr[256];

    getCommStr(comp->comm, commStr);
    getRanksStr(comp->ranks, comp->nbPes, verbose, ranksStr);
    App_Log(APP_DEBUG,
            "Component %s: \n"
            "  id:              %d\n"
            "  comm:            %s\n"
            "  nbPes:         %d\n"
            "  shared:  %d\n"
            "  ranks:           %s\n",
            App_MPMD_ComponentIdToName(comp->id), comp->id, commStr, comp->nbPes, comp->shared, ranksStr);
}


static void printSet(
    const TComponentSet* set
) {
    char ids_str[1024];
    char comm_str[32];

    getRanksStr(set->componentIds, set->nbComponents, 1, ids_str);
    getCommStr(set->comm, comm_str);

    App_Log(APP_INFO,
        "Set: \n"
        "  nbComponents: %d\n"
        "  components: %s\n"
        "  communicator: %s\n", set->nbComponents, ids_str, comm_str);
}
