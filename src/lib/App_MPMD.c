//! \file
//! Implementation of MPMD functions
//! \defgroup MPMD
//! MPI Multiple Program Multiple Data (MPMD) helper functions
//! @{
//! This MPMD module works with the rest of the App library.
//!
//! Each typical MPMD applications will do the following:
//! -# Call `MPI_Init()` to initialize MPI.
//! -# Call \ref App_Init() to initialize the application. The application name will be used as the MPMD component name.
//! -# Call \ref App_MPMD_Init()
//! -# Call \ref App_Start() to signal the beginning of the execution.
//! -# Call \ref App_MPMD_HasComponent() to confirm that the MPI execution context includes
//!     another component with which this application needs to exchange data.
//! -# Call \ref App_MPMD_GetSharedComm() to get a shared communicator between at least two components.
//! -# Do actual work with the shared communicator.
//! -# Call \ref App_End()
//! -# Call \ref App_MPMD_Finalize()
//! -# Call `MPI_Finalize()`
//!
//! To execute the applications in MPMD mode, an appropriate launch command for the MPI implementation must be used.
//!
//! Here is an example for OpenMPI:
//!
//! `mpirun -n1 mpmd_1 : -n4 mpmd_2`
//!
//! See the content of the `test` directory for examples of simple MPMD applications.


#include "App_MPMD.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

//! MPMD component identification
typedef struct {
    //! ID of this component
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

static void printComponent(const TComponent * const comp, const int verbose);


//! Create a new string containing a textual representation of an array of positive integers
static char * printIntArray(
    //! [in] Number of elements in the array
    const int nbElems,
    //! [in] Array to print
    const int array[nbElems],
    //! [in] Maximum number of elements to print. Use 0 to print all elements.
    const int maxElems
) {
    //! \note User must free the returned pointer after use

    int maxVal = 0;
    for (int i = 0; i < nbElems; i++) {
        if (array[i] > maxVal) maxVal = array[i];
    }
    const int width = maxVal == 0 ? 1 : log10f(maxVal) + 1;
    const int fmtWidth = log10f(width) + 1;
    char fmtStr[fmtWidth + 4];
    sprintf(fmtStr, "%%0%dd", width);

    const int nbPrint = 0 == maxElems ? nbElems : ( nbElems < maxElems ? nbElems : maxElems );
    char * const str = malloc(nbPrint * (width + 2) + 1);
    char * pos = str + sprintf(str, "{");
    for (int i = 0; i < nbPrint; i++) {
        pos = pos + sprintf(pos, fmtStr, array[i]);
        if (i < nbPrint - 1) {
            pos = pos + sprintf(pos, ", ");
        }
    }
    sprintf(pos, "}");

    return str;
}


//! Find unique components
static TComponent * findUniqueComponents(
    //! [in] Number of PE in MPI_COMM_WORLD
    const int worldSize,
    //! [in] List of component ID for each PE
    const TComponentId peComponentIds[worldSize],
    //! [out] Number of unique components
    int * const nbComponents
) {
    //! \return Pointer to a new sorted array of unique components.
    //! The index of the array corresponds to the component id/MPI_APPNUM and can therefore be addressed directly.
    //! The caller must free when no longer needed.

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
                    return(NULL);
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
    return app->SelfComponent->id;
}


//! Fill the provided buffer with a string indicating if the MPI communicator is NULL or not
static void getCommStr(
    //! [in] MPI communicator
    const MPI_Comm comm,
    //! [out] Output buffer. Must be at least 17 characters long
    char * const buffer
) {
    if (comm == MPI_COMM_NULL) {
        strcpy(buffer, "MPI_COMM_NULL");
    } else {
        strcpy(buffer, "[something]");
    }
}


//! Fill the provided buffer with a string representing the ranks
static void getRanksStr(
    //! [in] Number of elements in the ranks array
    const int nbRanks,
    //! [in] Array of the PE ranks
    const int ranks[nbRanks],
    //! [in] Print the actual ranks if true
    const int withNumbers,
    //! [out] Output buffer. Should be at least 8*15 characters long (120?)
    char * const buffer
) {
    if (ranks == NULL) {
        strcpy(buffer, "NULL");
    } else if (!withNumbers) {
        strcpy(buffer, "{...}");
    } else {
        const int MAX_PRINT = 15;
        char * const rankStr = printIntArray(nbRanks, ranks, MAX_PRINT);
        strcpy(buffer, rankStr);
        free(rankStr);
    }
}


//! Print a text representation of the provided component to the application log
static void printComponent(
    //! The component whose info we want to print
    const TComponent * const comp,
    //! Whether to print the global rank of every PE in the component
    const int verbose
) {
    char commStr[32];
    char ranksStr[256];

    getCommStr(comp->comm, commStr);
    getRanksStr(comp->nbPes, comp->ranks, verbose, ranksStr);
    App_Log(APP_DEBUG,
            "Component %s: \n"
            "  id:              %d\n"
            "  comm:            %s\n"
            "  nbPes:           %d\n"
            "  shared:          %d\n"
            "  ranks:           %s\n",
            App_MPMD_ComponentIdToName(comp->id), comp->id, commStr, comp->nbPes, comp->shared, ranksStr);
}


//! Initialize a common MPMD context by telling everyone who we are as a process
int App_MPMD_Init() {
    //! This is a collective call. Everyone who participate in it will know who else
    //! is on board and will be able to ask for a communicator in common with any other
    //! participant (or even multiple other participants at once).

    TApp * const app = App_GetInstance();

    #pragma omp single // For this entire function
    {
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &app->WorldRank);

#ifndef NDEBUG
        printf("%s: PE %d/%d, component %s\n", __func__, app->WorldRank, worldSize, app->Name);
#endif

        App_Log(APP_DEBUG, "%s: Initializing component %s PE %04d/%04d\n", __func__, app->Name, app->WorldRank, worldSize);

        app->MainComm = MPI_COMM_WORLD;

        // We need to assign a unique id for the component, but multiple mpi processors may share the same component name
        // This needs to be a collective call : MPI_Gather()
        // In order to allocate our input buffer, we must set a maximum component name length
        TComponentId * peComponentIds = NULL;
        if (app->WorldRank == 0) {
            peComponentIds = malloc(worldSize * sizeof(TComponentId));
        }

        int * appNum;
        {
            int flag;
            MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_APPNUM, &appNum, &flag);
        }
        TComponentId peComponentId = {.id = *appNum};
        strncpy(peComponentId.name, app->Name, APP_MAX_COMPONENT_NAME_LEN);
        MPI_Gather(&peComponentId, sizeof(TComponentId), MPI_BYTE, peComponentIds, sizeof(TComponentId), MPI_BYTE, 0, MPI_COMM_WORLD);
        if (app->WorldRank == 0) {
            app->AllComponents = findUniqueComponents(worldSize, peComponentIds, &(app->NumComponents));
            App_Log(APP_DEBUG, "%s: app->NumComponents = %d\n", __func__, app->NumComponents);
            free(peComponentIds);
        }
        MPI_Bcast(&(app->NumComponents), 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (app->WorldRank != 0) {
            app->AllComponents = malloc(app->NumComponents * sizeof(TComponent));
        }
        MPI_Bcast(app->AllComponents, app->NumComponents * sizeof(TComponent), MPI_BYTE, 0, MPI_COMM_WORLD);
        // At this point all processes have the same list of unique component id-names pairs

        const int componentId = *appNum;

        app->SelfComponent = &app->AllComponents[componentId];
        app->SelfComponent->id = componentId;
        MPI_Comm_split(MPI_COMM_WORLD, componentId, app->WorldRank, &app->SelfComponent->comm);
        MPI_Comm_rank(app->SelfComponent->comm, &app->ComponentRank);
        MPI_Comm_size(app->SelfComponent->comm, &app->SelfComponent->nbPes);

        // Declare that rank 0 of this component is "active" as a logger
        if (app->ComponentRank == 0) App_LogRank(app->WorldRank);
        // App_Log(APP_INFO, "%s: Initializing component %s (ID %d) with %d PEs\n", __func__, app->Name, componentId, app->SelfComponent->nbPes);

        if (app->WorldRank == 0 && app->ComponentRank != 0) {
            App_Log(APP_FATAL, "%s: Global root should also be the root of its own component\n", __func__);
            app->SelfComponent = NULL;
        } else {
            // Create a communicator with the roots of each component
            MPI_Comm rootsComm = MPI_COMM_NULL;
            int rootRank = -1; // Rank of the root in the rootsComm communicator
            int rootSize = -1;
            {
                const int isComponentRoot = app->ComponentRank == 0 ? 0 : MPI_UNDEFINED;
                // Use component ID as key so that the components are sorted in ascending order
                MPI_Comm_split(MPI_COMM_WORLD, isComponentRoot, app->WorldRank, &rootsComm);
                // The PEs that are not to roots get the MPI_COMM_NULL communicator which causes most MPI functions to fail
                if (rootsComm != MPI_COMM_NULL) {
                    MPI_Comm_rank(rootsComm, &rootRank);
                    MPI_Comm_size(rootsComm, &rootSize);
                }
            }

            // Each component root should have a list of the world rank of every other root
            // (we don't know which one is the WORLD root)
            int rootWorldRanks[app->NumComponents];
            if (app->ComponentRank == 0) {
                MPI_Allgather(&app->WorldRank, 1, MPI_INT, rootWorldRanks, 1, MPI_INT, rootsComm);
            }
            // Send the component roots to everyone
            MPI_Bcast(&rootWorldRanks, app->NumComponents, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast(&rootRank, 1, MPI_INT, 0, app->SelfComponent->comm);

            // Determine global ranks of the PEs in this component and share with other components

            // Self-component info that is only valid on a PE from this component
            app->SelfComponent->ranks = malloc(app->SelfComponent->nbPes * sizeof(int));

            // Each component gathers the world ranks of its members
            MPI_Allgather(&app->WorldRank, 1, MPI_INT, app->SelfComponent->ranks, 1, MPI_INT, app->SelfComponent->comm);
            app->SelfComponent->shared = 1;

    #ifndef NDEBUG
            printf("PE %d shared = %d\n", app->WorldRank, app->SelfComponent->shared);
    #endif
            // Share the number of PEs in each component
            for (int i = 0; i < app->NumComponents; i++) {
                // \todo Make sure that rootWorldRanks is in the same order as app->AllComponents
                MPI_Bcast(&app->AllComponents[i].nbPes, 1, MPI_INT, rootWorldRanks[i], MPI_COMM_WORLD);
            }

            {
                char rankStr[1024];
                char * pos = &rankStr[0];
                for (int i = 0;  i < app->SelfComponent->nbPes; i++) {
                    pos = pos + sprintf(pos, "%d", app->SelfComponent->ranks[i]);
                    if (i < app->SelfComponent->nbPes - 1) {
                        pos = pos + sprintf(pos, ", ");
                    }
                }
    #ifndef NDEBUG
                printf("PE %d : %d - %d/%d ranks = %s\n", app->WorldRank, app->SelfComponent->id, app->ComponentRank, app->SelfComponent->nbPes, rankStr);
    #endif
            }

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
                    printComponent(&app->AllComponents[i], 1);
                }
            }
        }

        // App_Start not called yet, means user wants App management per component, so we use the component communicator
        if (app->State==APP_STOP && app->SelfComponent) app->Comm=app->SelfComponent->comm;

    } // omp single

    return app->SelfComponent != NULL;
}


//! Terminate the MPMD execution cleanly
void App_MPMD_Finalize() {
    //! Frees the memory allocated by TComponent and TComponentSet structs, and it calls MPI_Finalize()
    #pragma omp single
    {
        TApp * const app = App_GetInstance();
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
                    free(app->AllComponents[i].ranks);
                }
            }
            MPI_Comm_free(&(app->SelfComponent->comm));
            free(app->AllComponents);
            app->AllComponents = NULL;
            app->SelfComponent = NULL;
            app->NumComponents = 0;
            app->MainComm = MPI_COMM_NULL;
        }
    } // omp single
}


//! Test if a list of integer contains a given item
static int contains(
    //! Number of items in the list
    const int nbItems,
    //! List in which to search
    const int list[nbItems],
    //! Item to search for
    const int item
) {
    //! \return 1 if a certain item is in the given list, 0 otherwise
    for (int i = 0; i < nbItems; i++) if (list[i] == item) return 1;
    return 0;
}


//! Print a list in the log with level APP_EXTRA
static void printList(
    //! [in] Number of items in the list
    const int size,
    //! [in] Array/List
    const int list[size]
) {
    if (App_LogLevel(NULL) >= APP_EXTRA) {
        char * const str = printIntArray(size, list, 0);
        App_Log(APP_EXTRA, "List: %s\n", str);
        free(str);
    }
}


//! Get a sorted list of components without duplication
static int * cleanComponentList(
    //! [in] How many components there are in the initial list
    const int nbComponents,
    //! [in] Initial list of component IDs
    const int components[nbComponents],
    //! [out] How many components there are in the cleaned-up list
    int * const nbUniqueComponents
) {
    //! \return Cleaned up list. Caller must free this list when no longer needed.

    int tmpList[nbComponents + 1];

    // Perform an insertion sort, while checking for duplicates
    for (int i = 0; i < nbComponents + 1; i++) tmpList[i] = -1;
    tmpList[0] = components[0];
    *nbUniqueComponents = 1;
    App_Log(APP_EXTRA, "nbComponents = %d\n", nbComponents);
    printList(nbComponents + 1, tmpList);
    for (int i = 1; i < nbComponents; i++) {
        const int item = components[i];

        App_Log(APP_EXTRA, "i = %d, *nbUniqueComponents = %d, item = %d\n", i, *nbUniqueComponents, item);
        printList(nbComponents + 1, tmpList);
        App_Log(APP_EXTRA, "duplicate? %d\n", contains(*nbUniqueComponents, tmpList, item));

        if (!contains(*nbUniqueComponents, tmpList, item)) {
            // Loop through sorted list (so far)
            for (int j = i - 1; j >= 0; j--) {
                App_Log(APP_EXTRA, "j = %d\n", j);
                if (tmpList[j] > item) {
                    // Not the right position yet
                    tmpList[j + 1] = tmpList[j];
                    App_Log(APP_EXTRA, "Changed list to... \n");
                    printList(nbComponents + 1, tmpList);

                    if (j == 0) {
                        // Last one, so we know the 1st slot is the right one
                        tmpList[j] = item;
                        (*nbUniqueComponents)++;

                        App_Log(APP_EXTRA, "Changed list again to... \n");
                        printList(nbComponents + 1, tmpList);
                    }
                } else if (tmpList[j] < item) {
                    // Found the right position
                    tmpList[j + 1] = item;
                    (*nbUniqueComponents)++;

                    App_Log(APP_EXTRA, "Correct pos, changed list to... \n");
                    printList(nbComponents + 1, tmpList);
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
MPI_Comm App_MPMD_GetSelfComm() {
    //! \return The communicator for all PEs part of the same component as this PE
    const TApp * const app = App_GetInstance();
    return app->SelfComponent->comm;
}


//! \copydoc App_MPMD_GetSelfComm
MPI_Fint App_MPMD_GetSelfComm_F() {
    return MPI_Comm_c2f(App_MPMD_GetSelfComm());
}


//! Find the component set that corresponds to the given list of IDs within this MPMD context.
static TComponentSet * findSet(
    //! [in] TApp instance in which we want to search
    const TApp * const app,
    //! [in] Number of components in the provided set
    const int nbComponents,
    //! [in] What components are in the set we want. *Must be sorted in ascending order and without duplicates*.
    const int components[nbComponents]
) {
    //! \return The set containing the given components (if found), or NULL if it wasn't found.

#ifndef NDEBUG
    {
        char * const compStr = printIntArray(nbComponents, components, 0);
        printf("%02d %s(%p, %s, %d)\n", app->WorldRank, __func__, (void *)app, compStr, nbComponents);
    }
#endif

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


//! Initialize the given component set with specific values
static void initComponentSet(
    //! Set to be initialized
    TComponentSet * const set,
    //! Number of components in the set
    const int nbComponents,
    //! List of component IDs that are part of the set
    const int componentIds[nbComponents],
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


//! Create a set of components within this MPMD context
static TComponentSet * createSet(
    //! TApp instance. *Must already be initialized.*
    TApp * const app,
    //! Number of components in the set
    const int nbComponents,
    //! List of components IDs in the set. *Must be sorted in ascending order and without duplicates*.
    const int components[nbComponents]
) {
    //! This will create a communicator for them.
    //! \return A newly-created set (pointer). NULL on error
#ifndef NDEBUG
    {
        char * const compStr = printIntArray(nbComponents, components, 0);
        printf("%02d %s(%p, %s, %d)\n", app->WorldRank, __func__, (void *)app, compStr, nbComponents);
    }
#endif

    if (app->Sets == NULL) {
        // Choose (arbitrarily) initial array size to be the total number of components in the context
        app->Sets = (TComponentSet*)malloc(app->NumComponents * sizeof(TComponentSet));
        app->SizeSets = app->NumComponents;
    }

    // Make sure the array is large enough to contain the new set.
    // We need to allocate a new (larger) one if not, and transfer all existing sets to it
    if (app->NbSets >= app->SizeSets) {
        const int oldSize = app->SizeSets;
        const int newSize = oldSize * 2;

        TComponentSet * const newSets = (TComponentSet*)malloc(newSize * sizeof(TComponentSet));
        memcpy(newSets, app->Sets, oldSize * sizeof(TComponentSet));
        free(app->Sets);
        app->Sets = newSets;
        app->SizeSets = newSize;
    }

    TComponentSet * const newSet = &app->Sets[app->NbSets];
    *newSet = defaultSet;

    // Share the world ranks of other components among all PEs of this component (if not done already)
    for (int i = 0; i < nbComponents; i++) {
        TComponent * const comp = &app->AllComponents[components[i]];
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

        int newGroupSize = 0;
        for (int i = 0; i < nbComponents; i++) {
            newGroupSize += app->AllComponents[components[i]].nbPes;
        }

        int * const newGroupRanks = (int*) malloc(newGroupSize * sizeof(int));
        int currentPe = 0;
        for (int i = 0; i < nbComponents; i++) {
            const TComponent* comp = &(app->AllComponents[components[i]]);
            memcpy(newGroupRanks + currentPe, comp->ranks, comp->nbPes * sizeof(int));
            currentPe += comp->nbPes;
        }

        MPI_Group_incl(mainGroup, newGroupSize, newGroupRanks, &unionGroup);

        // This call is collective among all group members, but not main_comm
        MPI_Comm_create_group(app->MainComm, unionGroup, 0, &unionComm);

        // Initialize in place, in the list of sets
        initComponentSet(newSet, nbComponents, components, unionComm, unionGroup);
        app->NbSets++;

        free(newGroupRanks);
    }

#ifndef NDEBUG
    MPI_Barrier(newSet->comm);
    printf("%02d %s : return newSet\n", app->WorldRank, __func__);
#endif

    return newSet;
}


//! Get the string (name) that corresponds to the given component ID
const char * App_MPMD_ComponentIdToName(
    //! [in] Component ID
    const int componentId
) {
    //! \return Pointer to the string containing the component name. Caller must **not** free this pointer.

    const TApp * const app = App_GetInstance();

    if (0 <= componentId && componentId < app->NumComponents) {
        return app->AllComponents[componentId].name;
    } else {
        return NULL;
    }
}


//! Get a communicator that encompasses all PEs part of the components in the given list
MPI_Comm App_MPMD_GetSharedComm(
    //! Number of components in the list
    const int32_t nbComponents,
    //! The list of components IDs for which we want a shared communicator.
    //! This list *must* contain the component of the calling PE. It may contain
    //! duplicate IDs and does not have to be in a specific order.
    const int32_t components[nbComponents]
) {
    //!  If the communicator does not already exist, it will be created.
    //! _This function call is collective if and only if the communicator gets created._

    TApp * const app = App_GetInstance();
    char * const compStr = printIntArray(nbComponents, components, 0);
#ifndef NDEBUG
        printf("%02d %s(%p, %s, %d) from %d(%s)\n", app->WorldRank, __func__, (void *)app, compStr, nbComponents, app->SelfComponent->id, app->SelfComponent->name);
#endif
    MPI_Comm sharedComm = MPI_COMM_NULL;

    // Sanitize the list of components
    int nbUniqueComponents = 0;
    int * const uniqueComponents = cleanComponentList(nbComponents, components, &nbUniqueComponents);

    char * const uniqueStr = printIntArray(nbUniqueComponents, uniqueComponents, 0);
    App_Log(APP_DEBUG, "%s: Retrieving/creating shared comm for components %s (%s)\n", __func__, compStr, uniqueStr);

    // Make sure there are enough components in the list
    if (nbUniqueComponents < 2) {
        App_Log(APP_ERROR, "%s: There need to be at least 2 components (including this one) to share a communicator\n",
                __func__);
        goto end;
    }

    // Make sure this component is included in the list
    int found = 0;
    for (int i = 0; i < nbUniqueComponents; i++) {
        if (uniqueComponents[i] == app->SelfComponent->id) {
            found = 1;
            break;
        }
    }
    if (!found) {
#ifndef NDEBUG
        printf("%02d %s : Comp %d(%s) not in %s!\n", app->WorldRank, __func__, app->SelfComponent->id, app->SelfComponent->name, compStr);
#endif
        App_Log(APP_WARNING, "%s: You must include self component (%d[%s]) in the list of components"
            " when requesting a shared communicator!\n Requested %s\n", __func__, app->SelfComponent->id, app->SelfComponent->name, compStr);
        goto end;
    }

    // Check whether a communicator has already been created for this set of components
    {
        const TComponentSet * set = findSet(app, nbUniqueComponents, uniqueComponents);
        if (set) {
#ifndef NDEBUG
            printf("%02d (PE %02d of comp %s) Found already existing set for %s\n",
                app->WorldRank, app->ComponentRank, app->SelfComponent->name, compStr);
#endif
            App_Log(APP_DEBUG, "%s: Found already existing set at %p\n", __func__, set);
            sharedComm = set->comm;
            goto end;
        }
    }

    // Not created yet, so we have to do it now
    const TComponentSet * const set = createSet(app, nbUniqueComponents, uniqueComponents);

    if (set == NULL) {
        // Oh nooo, something went wrong
        App_Log(APP_ERROR, "%s: Unable to create a communicator for the given set (%s)\n", __func__, compStr);
    } else {
        sharedComm = set->comm;
    }

end:
    if (sharedComm == MPI_COMM_NULL) {
        App_Log(APP_ERROR, "%s: Communicator is NULL for components %s\n", __func__, compStr);
    }

    free(uniqueComponents);
    free(compStr);
    free(uniqueStr);
#ifndef NDEBUG
    printf("%02d %s return sharedComm\n", app->WorldRank, __func__);
#endif
    return sharedComm;
}


//! Get shared Fortran communicator
MPI_Fint App_MPMD_GetSharedComm_F(
    //! Number of components
    const int32_t nbComponents,
    //! List of component IDs that should be grouped
    const int32_t components[nbComponents]
) {
    //! \return The Fortran communicator shared by the given components
    return MPI_Comm_c2f(App_MPMD_GetSharedComm(nbComponents, components));
}


//! Test if the provided component is present in this MPMD context
int32_t App_MPMD_HasComponent(
    //! [in] Component name
    const char * const componentName
) {
    //! \return 1 if given component is present in this MPMD context, 0 if not.
   
    App_Log(APP_DEBUG, "%s: Checking for presence of component \"%s\" ...\n", __func__, componentName);

    return App_MPMD_GetComponentId(componentName) >= 0;
}


//! @}
