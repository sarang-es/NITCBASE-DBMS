#include "OpenRelTable.h"
#include <cstdlib>
#include <cstring>

OpenRelTable::OpenRelTable()
{
    // ==========================================================
    // 1. INITIALIZE RELATION CACHE AND ATTRIBUTE CACHE
    // ==========================================================
    // Initially, all cache entries are empty.
    // So initialize every entry to NULL.
    // ==========================================================

    for(int i = 0; i < MAX_OPEN; i++)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }


    // ==========================================================
    // 2. SET UP RELATION CACHE
    // ==========================================================

    // Create a buffer for the RELCAT block.
    // RELCAT contains information about all relations.
    RecBuffer relCatBlock(RELCAT_BLOCK);

    // This array stores one RELCAT record.
    Attribute relCatRecord[RELCAT_NO_ATTRS];


    // ----------------------------------------------------------
    // 2.1 SET UP RELCAT
    // ----------------------------------------------------------
    // Read the RELCAT record which describes RELCAT itself.
    // Convert it into a RelCatEntry and store it in the
    // relation cache at RELCAT_RELID.
    // ----------------------------------------------------------

    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_RELCAT
    );

    struct RelCacheEntry relCacheEntry;

    // Convert raw RELCAT record into RelCatEntry.
    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );

    // Store the location of RELCAT's record.
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    // Allocate memory for RELCAT's cache entry.
    RelCacheTable::relCache[RELCAT_RELID] =
        (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));

    // Copy the temporary entry into the cache.
    *(RelCacheTable::relCache[RELCAT_RELID]) =
        relCacheEntry;


    // ----------------------------------------------------------
    // 2.2 SET UP ATTRCAT
    // ----------------------------------------------------------
    // Read the RELCAT record which describes ATTRCAT.
    // Convert it into a RelCatEntry and store it in the
    // relation cache at ATTRCAT_RELID.
    // ----------------------------------------------------------

    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_ATTRCAT
    );

    // Convert ATTRCAT's RELCAT record.
    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );

    // Store the location of ATTRCAT's record.
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    // Allocate memory for ATTRCAT's cache entry.
    RelCacheTable::relCache[ATTRCAT_RELID] =
        (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));

    // Copy the temporary entry into the cache.
    *(RelCacheTable::relCache[ATTRCAT_RELID]) =
        relCacheEntry;


    // ----------------------------------------------------------
    // 2.3 SET UP SAMPLE TABLE
    // ----------------------------------------------------------
    // Exercise Stage 3 Q1
    //
    // The sample table has relation ID = 2.
    //
    // Therefore, the RELCAT record at slot 2 describes
    // the sample table.
    // ----------------------------------------------------------

    relCatBlock.getRecord(
        relCatRecord,
        2
    );

    // Convert the sample table's RELCAT record
    // into a RelCatEntry.
    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );

    // Store the location of the sample table's
    // RELCAT record.
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = 2;

    // Allocate a relation-cache entry for relation ID 2.
    RelCacheTable::relCache[2] =
        (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));

    // Copy the sample table information into the cache.
    *(RelCacheTable::relCache[2]) =
        relCacheEntry;

    // End of sample table relation-cache setup.



    // ==========================================================
    // 3. SET UP ATTRIBUTE CACHE
    // ==========================================================

    // Create a buffer for ATTRCAT.
    // ATTRCAT contains information about the attributes
    // of all relations.
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);

    // head points to the first node of the attribute list.
    // temp points to the last node.
    struct AttrCacheEntry* head = NULL;
    struct AttrCacheEntry* temp = NULL;


    // ----------------------------------------------------------
    // 3.1 ATTRIBUTES OF RELCAT
    // ----------------------------------------------------------
    // The first 6 records of ATTRCAT belong to RELCAT.
    //
    // ATTRCAT slots:
    //     0, 1, 2, 3, 4, 5
    //
    // These attributes are linked together and stored in:
    //
    //     attrCache[RELCAT_RELID]
    // ----------------------------------------------------------

    for(int i = 0; i < 6; i++)
    {
        // Store one ATTRCAT record.
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        // Read the attribute catalog record.
        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );

        // Temporary attribute-cache entry.
        struct AttrCacheEntry attrCacheEntry;

        // Convert the raw record into AttrCatEntry.
        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &attrCacheEntry.attrCatEntry
        );

        // Store the location of the attribute record.
        attrCacheEntry.recId.block = RELCAT_BLOCK;
        attrCacheEntry.recId.slot = i;

        // Initially, this node does not point to another node.
        attrCacheEntry.next = NULL;

        // Allocate memory for a new linked-list node.
        struct AttrCacheEntry* temp2 =
            (struct AttrCacheEntry*)malloc(
                sizeof(AttrCacheEntry)
            );

        // Copy the attribute information into the node.
        *(temp2) = attrCacheEntry;


        // If this is the first attribute,
        // make it the head of the list.
        if(!head)
        {
            head = temp2;
            temp = temp2;
        }
        else
        {
            // Attach the new attribute to the end.
            temp->next = temp2;

            // Move temp to the new last node.
            temp = temp->next;
        }
    }

    // Store RELCAT's attribute linked list.
    AttrCacheTable::attrCache[RELCAT_RELID] = head;



    // ----------------------------------------------------------
    // 3.2 ATTRIBUTES OF ATTRCAT
    // ----------------------------------------------------------
    // ATTRCAT's attributes occupy ATTRCAT slots 6 to 11.
    //
    // ATTRCAT slots:
    //     6, 7, 8, 9, 10, 11
    //
    // These are linked together and stored in:
    //
    //     attrCache[ATTRCAT_RELID]
    // ----------------------------------------------------------

    // Start a new linked list.
    head = NULL;
    temp = NULL;

    for(int i = 6; i < 12; i++)
    {
        // Store one ATTRCAT record.
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        // Read the attribute record.
        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );

        // Temporary attribute-cache entry.
        struct AttrCacheEntry attrCacheEntry;

        // Convert the raw record into AttrCatEntry.
        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &attrCacheEntry.attrCatEntry
        );

        // Store the location of the attribute record.
        attrCacheEntry.recId.block = ATTRCAT_BLOCK;
        attrCacheEntry.recId.slot = i;

        // Initially, no next node.
        attrCacheEntry.next = NULL;

        // Allocate memory for a new linked-list node.
        struct AttrCacheEntry* temp2 =
            (struct AttrCacheEntry*)malloc(
                sizeof(AttrCacheEntry)
            );

        // Copy the attribute information into the node.
        *(temp2) = attrCacheEntry;


        // If this is the first attribute,
        // make it the head.
        if(!head)
        {
            head = temp2;
            temp = temp2;
        }
        else
        {
            // Add the new attribute to the end.
            temp->next = temp2;

            // Move temp to the new last node.
            temp = temp->next;
        }
    }

    // Store ATTRCAT's attribute linked list.
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;



    // ==========================================================
    // 3.3 ATTRIBUTES OF SAMPLE TABLE
    // ==========================================================
    // Exercise Stage 3 Q1
    //
    // The sample table has:
    //
    //     Relation ID = 2
    //     Number of attributes = 2
    //
    // Its two attributes are stored in ATTRCAT slots:
    //
    //     12
    //     13
    //
    // These two attribute entries are linked together and
    // stored in:
    //
    //     attrCache[2]
    // ==========================================================

    // Start a new linked list.
    head = NULL;
    temp = NULL;

    // Read the 2 attributes of the sample table.
    for(int i = 12; i < 14; i++)
    {
        // Store one ATTRCAT record.
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        // Read the attribute record from ATTRCAT.
        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );

        // Temporary attribute-cache entry.
        struct AttrCacheEntry attrCacheEntry;

        // Convert the raw ATTRCAT record into AttrCatEntry.
        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &attrCacheEntry.attrCatEntry
        );

        // Store the location of the attribute record.
        attrCacheEntry.recId.block = ATTRCAT_BLOCK;
        attrCacheEntry.recId.slot = i;

        // Initially, this node points to nothing.
        attrCacheEntry.next = NULL;

        // Allocate memory for the new linked-list node.
        struct AttrCacheEntry* temp2 =
            (struct AttrCacheEntry*)malloc(
                sizeof(AttrCacheEntry)
            );

        // Copy the attribute information into the node.
        *(temp2) = attrCacheEntry;


        // If this is the first attribute,
        // make it the head of the list.
        if(!head)
        {
            head = temp2;
            temp = temp2;
        }
        else
        {
            // Attach the second attribute after the first.
            temp->next = temp2;

            // Move temp to the second attribute.
            temp = temp->next;
        }
    }

    // Store the sample table's attribute linked list
    // in the attribute cache at relation ID 2.
    AttrCacheTable::attrCache[2] = head;

    // End of Stage 3 Q1.
}



// ==========================================================
// DESTRUCTOR
// ==========================================================

OpenRelTable::~OpenRelTable()
{
    // Go through every possible relation-cache entry.
    for(int i = 0; i < MAX_OPEN; i++)
    {
        // --------------------------------------------------
        // Free relation-cache entry
        // --------------------------------------------------

        if(RelCacheTable::relCache[i] != nullptr)
        {
            // Free the dynamically allocated relation-cache entry.
            free(RelCacheTable::relCache[i]);

            // Reset the pointer.
            RelCacheTable::relCache[i] = nullptr;
        }


        // --------------------------------------------------
        // Free attribute-cache linked list
        // --------------------------------------------------

        // Get the first attribute of this relation.
        struct AttrCacheEntry* curr =
            AttrCacheTable::attrCache[i];


        // Traverse the linked list and free every node.
        while(curr != nullptr)
        {
            // Save the next node before freeing the current node.
            struct AttrCacheEntry* next =
                curr->next;

            // Free the current attribute-cache node.
            free(curr);

            // Move to the next node.
            curr = next;
        }

        // Mark the attribute-cache entry as empty.
        AttrCacheTable::attrCache[i] = nullptr;
    }
}