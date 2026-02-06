//! \file
//! Implementation of MPMD functions
//! \defgroup MPMD MPMD
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
//! See the content of the `test` directory in the source tree for examples of simple MPMD applications.


#include "App_MPMD.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <linux/limits.h>


//! MPMD component identification
typedef struct {
    //! ID of this component
    int id;
    //! Name of the component
    char name[APP_MAX_COMPONENT_NAME_LEN];
    //! Process rank in MPI_COMM_WORLD
    int worldRank;
    //! Processor Name (host name)
    char processorName[MPI_MAX_PROCESSOR_NAME];
} TComponentMap;


//! Default values for a TComponentSet struct
static const TComponentSet defaultSet = {
    .nbComponents = 0,
    .componentIds = NULL,
    .nbPes = 0,
    .comm = MPI_COMM_NULL,
    .group = MPI_GROUP_NULL
};


//! Create a new string containing a textual representation of an array of positive integers
static char * intArrayStr(
    //! [in] Number of elements in the array
    const int nbElems,
    //! [in] Array to print
    const int array[nbElems],
    //! [in] Maximum number of elements to print. Use 0 to print all elements.
    const int maxElems
) {
    //! \return Pointer to the string representing the array or NULL on error
    //! \note User must free the returned pointer after use

    if (!array) return NULL;
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
    if (str) {
        char * pos = str + sprintf(str, "{");
        for (int i = 0; i < nbPrint; i++) {
            pos = pos + sprintf(pos, fmtStr, array[i]);
            if (i < nbPrint - 1) {
                pos = pos + sprintf(pos, ", ");
            }
        }
        sprintf(pos, "}");
    }

    return str;
}


//! Find unique components
static TComponent * findUniqueComponents(
    //! [in] Number of PE in MPI_COMM_WORLD
    const int worldSize,
    //! [in] List of component ID for each PE
    const TComponentMap peComponentIds[worldSize],
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
        // Initialize the component attributes to make debugging easier
        uniqueComponents[compIdx].comm = MPI_COMM_NULL;
        uniqueComponents[compIdx].size = -1;
        uniqueComponents[compIdx].pe0WorldRank = -1;
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


//! Get component size (number of processes)
int App_MPMD_GetComponentSize(
    //! [in] Component id
    const int componentId
) {
    //! \return Component size if found, -1 otherwise

    const TApp * const app = App_GetInstance();

    if (componentId >= 0 && componentId < app->NumComponents) {
        return app->AllComponents[componentId].size;
    }
    return -1;
}


//! Get component process' world rank
int App_MPMD_GetComponentPeWRank(
    //! [in] Component id
    const int componentId,
    //! [in] Rank within the component
    const int localRank
) {
    //! \return Process' world rank if found, -1 otherwise

    const TApp * const app = App_GetInstance();

    if (localRank >= 0 && localRank < app->SelfComponent->size) {
        return app->SelfComponent->pe0WorldRank + localRank;
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
static char * getRanksStr(
    //! [in] Component size
    const int size,
    //! [in] Array of the PE ranks
    const int pe0WorldRank,
    //! [in] Print the actual ranks if true
    const int withNumbers
) {
    //! \return Pointer to the string containing the ranks. Caller must free this pointer when no longer needed.

    if (!withNumbers) {
        char * const rankStr = malloc(6 * sizeof(char));
        strcpy(rankStr, "{...}");
        return rankStr;
    } else {
        const int MAX_PRINT = 15;
        int * const ranks = malloc(size * sizeof(int));
        for (int i = 0; i < size; i++) {
            ranks[i] = pe0WorldRank + i;
        }
        char * const rankStr = intArrayStr(size, ranks, MAX_PRINT);
        free(ranks);
        return rankStr;
    }
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


//! Print a text representation of the provided component to the application log
static void printComponent(
    //! [in] The component whose info we want to print
    const TComponent * const comp,
    //! [in] Whether to print the global rank of every PE in the component
    const int verbose
) {
    char commStr[32];

    getCommStr(comp->comm, commStr);
    char * const ranksStr = getRanksStr(comp->size, comp->pe0WorldRank, verbose);
    App_Log(APP_DEBUG,
            "Component %s: \n"
            "  id:              %d\n"
            "  comm:            %s\n"
            "  size:            %d\n"
            "  ranks:           %s\n",
            App_MPMD_ComponentIdToName(comp->id), comp->id, commStr, comp->size, ranksStr);
    free(ranksStr);
}


//! Print component map
static int App_MPMD_PrintComponentMap(
    //! [in] Number of processes in MPI_COMM_WORLD
    const int size,
    //! [in] Component map
    const TComponentMap map[size],
    //! [in] Csv file path or NULL to print in the application log
    const char filePath[PATH_MAX]
) {
    //! The component map is written in csv format in no particular order.
    //! If NULL is provided instead of a file path, the map is written to the application log with the log level APP_INFO.

    //! \return 1 on success, 0 otherwise
    const char header[] = "Process Rank, MPI_APPNUM, Component Name, Hostname\n";

    if (filePath) {
        FILE * fd = fopen(filePath, "w");
        if (fd) {
            fprintf(fd, header);
            for (int i = 0; i < size; i++) {
                fprintf(fd, "%06d, %1d, \"%s\", \"%s\"\n", map[i].worldRank, map[i].id, map[i].name, map[i].processorName);
            }
            fclose(fd);
        } else {
            App_Log(APP_ERROR, "Could not open output file (%s) to write component map!\n", filePath);
            return 0;
        }
    } else {
        App_Log(APP_INFO, header);
        for (int i = 0; i < size; i++) {
            App_Log(APP_INFO, "%06d, %1d, \"%s\", \"%s\"\n", map[i].worldRank, map[i].id, map[i].name, map[i].processorName);
        }
    }
    return 1;
}


//! Initialize a common MPMD context by telling everyone who we are as a process
int App_MPMD_Init() {
    //! This is a collective call. Everyone who participate in it will know who else
    //! is on board and will be able to ask for a communicator in common with any other
    //! participant (or even multiple other participants at once).

    //! \return 1 on success, 0 otherwise

    TApp * const app = App_GetInstance();

    #pragma omp single // For this entire function
    {
        int worldSize;
        MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
        MPI_Comm_rank(MPI_COMM_WORLD, &app->WorldRank);

        App_Log(APP_DEBUG, "%s: Initializing component %s PE %04d/%04d\n", __func__, app->Name, app->WorldRank, worldSize);

        app->MainComm = MPI_COMM_WORLD;

        // We need to assign a unique id for the component, but multiple mpi processors may share the same component name
        // This needs to be a collective call : MPI_Gather()
        // In order to allocate our input buffer, we must set a maximum component name length
        TComponentMap * peComponentIds = NULL;
        if (app->WorldRank == 0) {
            peComponentIds = malloc(worldSize * sizeof(TComponentMap));
        }

        int * appNum;
        {
            int flag;
            MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_APPNUM, &appNum, &flag);
        }
        App_Log(APP_DEBUG, "%s: %06d/%06d, component \"%s\" MPI_APPNUM = %d\n", __func__, app->WorldRank, worldSize, app->Name, *appNum);

        TComponentMap peComponentId = {.id = *appNum, .worldRank = app->WorldRank};
        {
            int processorNameLen;
            MPI_Get_processor_name(peComponentId.processorName, &processorNameLen);
        }
        strncpy(peComponentId.name, app->Name, APP_MAX_COMPONENT_NAME_LEN);
        MPI_Gather(&peComponentId, sizeof(TComponentMap), MPI_BYTE, peComponentIds, sizeof(TComponentMap), MPI_BYTE, 0, MPI_COMM_WORLD);
        if (app->WorldRank == 0) {
            app->AllComponents = findUniqueComponents(worldSize, peComponentIds, &(app->NumComponents));
            App_Log(APP_DEBUG, "%s: app->NumComponents = %d\n", __func__, app->NumComponents);
            App_MPMD_PrintComponentMap(worldSize, peComponentIds, NULL);
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
        MPI_Comm_size(app->SelfComponent->comm, &app->SelfComponent->size);

        if (app->ComponentRank == 0) {
            // Declare that rank 0 of this component is "active" as a logger
            App_LogRank(app->WorldRank);
            // App_Log(APP_INFO, "%s: Initializing component %s (ID %d) with %d PEs\n", __func__, app->Name, componentId, app->SelfComponent->size);
        }

        if (app->WorldRank == 0 && app->ComponentRank != 0) {
            //! \todo Remove this? How would it even be possible?
            App_Log(APP_FATAL, "%s: Global root should also be the root of its own component\n", __func__);
            app->SelfComponent = NULL;
        } else {
            // Create a communicator with the roots of each component
            MPI_Comm rootsComm = MPI_COMM_NULL;
            {
                const int isComponentRoot = app->ComponentRank == 0 ? 0 : MPI_UNDEFINED;
                // Use component ID as key so that the components are sorted in ascending order
                MPI_Comm_split(MPI_COMM_WORLD, isComponentRoot, app->WorldRank, &rootsComm);
            }

            // Each component root should have a list of the world rank of every other root
            // (we don't know which one is the WORLD root)
            int rootWorldRanks[app->NumComponents];
            if (app->ComponentRank == 0) {
                MPI_Allgather(&app->WorldRank, 1, MPI_INT, rootWorldRanks, 1, MPI_INT, rootsComm);
            }
            // Send the component roots to everyone
            MPI_Bcast(&rootWorldRanks, app->NumComponents, MPI_INT, 0, MPI_COMM_WORLD);

            // Share the number of PEs in each component and the world rank of each component's PE0
            for (int i = 0; i < app->NumComponents; i++) {
                MPI_Bcast(&app->AllComponents[i].size, 1, MPI_INT, rootWorldRanks[i], MPI_COMM_WORLD);
                app->AllComponents[i].pe0WorldRank = rootWorldRanks[i];
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
        if (app->State == APP_STOP && app->SelfComponent) app->Comm = app->SelfComponent->comm;

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
                    if (app->Sets[i].comm != MPI_COMM_NULL) MPI_Comm_free(&(app->Sets[i].comm));
                    if (app->Sets[i].group != MPI_GROUP_EMPTY) MPI_Group_free(&(app->Sets[i].group));
                }
                free(app->Sets);
                app->Sets = NULL;
            }
            app->NbSets = 0;

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
    //! [in] Number of items in the list
    const int nbItems,
    //! [in] List in which to search
    const int list[nbItems],
    //! [in] Item to search for
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
        char * const str = intArrayStr(size, list, 0);
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
    const int components[nbComponents],
    //! [in] Number of PEs in the set to find. Set to -1 to ignore this check.
    const int nbPes
) {
    //! \return The set containing the given components (if found), or NULL if it wasn't found.

#ifndef NDEBUG
    {
        char * const compStr = intArrayStr(nbComponents, components, 0);
        printf("%02d %s(%p, %s, %d, %d)\n", app->WorldRank, __func__, (void *)app, compStr, nbComponents, nbPes);
        free(compStr);
    }
#endif

    for (int setIdx = 0; setIdx < app->NbSets; setIdx++) {
        TComponentSet * const set = &app->Sets[setIdx];
        if (set->nbComponents == nbComponents && (nbPes == -1 || set->nbPes == nbPes)) {
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
    //! [in, out] Set to be initialized
    TComponentSet * const set,
    //! [in] Number of components in the set
    const int nbComponents,
    //! [in] List of component IDs that are part of the set
    const int componentIds[nbComponents],
    //! [in] Number of PEs in the set
    const int nbPes,
    //! [in] MPI communicator that is common (and exclusive) to the given components
    const MPI_Comm comm,
    //! [in] MPI group that contains all (global) PEs from the given components
    const MPI_Group group
) {
    #ifndef NDEBUG
    {
        char * const compStr = intArrayStr(nbComponents, componentIds, 0);
        printf("%s(%s, %d, %d, %d, %d)\n", __func__, compStr, nbComponents, nbPes, comm, group);
        free(compStr);
    }
    #endif

    // When calling this function for a set containing the PE0s only, not all PEs will be
    // part of the new set. Therefore, to group for those process will be MPI_GROUP_EMPTY and
    // the communicator will be MPI_COMM_NULL

    set->nbComponents = nbComponents;
    set->componentIds = (int*)malloc(nbComponents * sizeof(int));
    memcpy(set->componentIds, componentIds, nbComponents * sizeof(int));
    set->nbPes = nbPes;
    set->comm = comm;
    set->group = group;
}


//! Create a set of components within this MPMD context
static TComponentSet * createSet(
    //! [in, out] TApp instance. *Must already be initialized.*
    TApp * const app,
    //! [in] Number of components in the set
    const int nbComponents,
    //! [in] List of components IDs in the set. *Must be sorted in ascending order and without duplicates*.
    const int components[nbComponents],
    //! [in] Include only PE0 of each component in the new set
    const int pes0Only
) {
    //! This will create a communicator for them.
    //! \return A newly-created set (pointer). NULL on error
    #ifndef NDEBUG
    {
        char * const compStr = intArrayStr(nbComponents, components, 0);
        printf("%02d %s(%p, %s, %d, %d)\n", app->WorldRank, __func__, (void *)app, compStr, nbComponents, pes0Only);
        free(compStr);
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

    // Create the set
    {
        MPI_Group mainGroup = MPI_GROUP_EMPTY;
        MPI_Group unionGroup = MPI_GROUP_EMPTY;
        MPI_Comm unionComm = MPI_COMM_NULL;

        MPI_Comm_group(app->MainComm, &mainGroup);

        int newGroupSize = 0;
        if (pes0Only) {
            if (app->ComponentRank == 0) {
                newGroupSize = nbComponents;

                int * const newGroupRanks = (int*) malloc(newGroupSize * sizeof(int));

                for (int i = 0; i < nbComponents; i++) {
                    newGroupRanks[i] = app->AllComponents[components[i]].pe0WorldRank;
                }
                MPI_Group_incl(mainGroup, newGroupSize, newGroupRanks, &unionGroup);
                free(newGroupRanks);

                // This call is collective among all group members, but not main_comm
                MPI_Comm_create_group(app->MainComm, unionGroup, 0, &unionComm);
            }
        } else {
            for (int i = 0; i < nbComponents; i++) {
                newGroupSize += app->AllComponents[components[i]].size;
            }

            int * const newGroupRanks = (int*) malloc(newGroupSize * sizeof(int));
            //! Build the list of the component's world ranks
            int currentPe = 0;
            for (int i = 0; i < nbComponents; i++) {
                const TComponent * const comp = &(app->AllComponents[components[i]]);
                for (int localRank = 0; localRank < comp->size; localRank++) {
                    newGroupRanks[currentPe++] = comp->pe0WorldRank + localRank;
                }
            }
            MPI_Group_incl(mainGroup, newGroupSize, newGroupRanks, &unionGroup);
            free(newGroupRanks);

            // This call is collective among all group members, but not main_comm
            MPI_Comm_create_group(app->MainComm, unionGroup, 0, &unionComm);
        }

        // Initialize in place, in the list of sets
        initComponentSet(newSet, nbComponents, components, newGroupSize, unionComm, unionGroup);
        app->NbSets++;
    }

    #ifndef NDEBUG
        printf("%02d %s : return newSet\n", app->WorldRank, __func__);
    #endif

    return newSet;
}


//! Get a communicator that encompasses all PEs or only the PE0 of the components in the given list
MPI_Comm App_MPMD_GetSharedComm(
    //! [in] Number of components in the list
    const int32_t nbComponents,
    //! [in] The list of components IDs for which we want a shared communicator.
    //! This list *must* contain the component of the calling PE. It may contain
    //! duplicate IDs and does not have to be in a specific order.
    const int32_t components[nbComponents],
    //! [in] Include only the PE0 of each component in the communicator
    const int32_t pes0Only
) {
    //!  If the communicator does not already exist, it will be created.
    //! \note This function call is collective if and only if the communicator gets created.

    // Can do much if the list of component is NULL. This also prevents the warning about CWE-690
    if (components == NULL) return MPI_COMM_NULL;
    TApp * const app = App_GetInstance();
    char * const compStr = intArrayStr(nbComponents, components, 0);
    #ifndef NDEBUG
        printf("%02d %s(%p, %s, %d, %d) from %d(%s)\n", app->WorldRank, __func__,
            (void *)app, compStr, nbComponents, pes0Only, app->SelfComponent->id, app->SelfComponent->name);
    #endif
    MPI_Comm sharedComm = MPI_COMM_NULL;

    // Sanitize the list of components
    int nbUniqueComponents = 0;
    int * const uniqueComponents = cleanComponentList(nbComponents, components, &nbUniqueComponents);

    char * const uniqueStr = intArrayStr(nbUniqueComponents, uniqueComponents, 0);
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
        // Compute the total number of PEs in the unique components
        int nbPe = 0;
        for (int i = 0; i < nbUniqueComponents; i++) {
            for (int j = 0; j < app->NumComponents; j++) {
                if (app->AllComponents[j].id == uniqueComponents[i]) {
                    nbPe += app->AllComponents[j].size;
                    break;
                }
            }
        }

        const TComponentSet * set = findSet(app, nbUniqueComponents, uniqueComponents, pes0Only ? nbUniqueComponents : nbPe);
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
    const TComponentSet * const set = createSet(app, nbUniqueComponents, uniqueComponents, pes0Only);

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
    //! [in] Number of components
    const int32_t nbComponents,
    //! [in] List of component IDs that should be grouped
    const int32_t components[nbComponents],
    //! [in] Include only the PE0 of each component in the communicator
    const int32_t pes0Only
) {
    //! \return The Fortran communicator shared by the given components
    return MPI_Comm_c2f(App_MPMD_GetSharedComm(nbComponents, components, pes0Only));
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


//! Get the rank of the current process in its component communicator
int App_MPMD_GetSelfComponentRank() {
    //! \return Rank of the current process in its component communicator
    const TApp * const app = App_GetInstance();
    return app->ComponentRank;
}


//! Get the size (number of processes) of the component's communicator
int App_MPMD_GetSelfComponentSize() {
    //! \return Size (number of processes) of the component's communicator
    const TApp * const app = App_GetInstance();
    return app->SelfComponent->size;
}
//! @}
