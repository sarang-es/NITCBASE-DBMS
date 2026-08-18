#include "OpenRelTable.h"
#include "RelCacheTable.h"
#include "AttrCacheTable.h"

#include <cstdlib>
#include <cstring>


OpenRelTable::OpenRelTable() {

    // --------------------------------------------------
    // 1. Initialize all relation and attribute cache
    //    entries to nullptr.
    // --------------------------------------------------

    for (int i = 0; i < MAX_OPEN; ++i) {

        RelCacheTable::relCache[i] = nullptr;

        AttrCacheTable::attrCache[i] = nullptr;
    }


    // ==================================================
    // 2. SET UP RELATION CACHE
    // ==================================================


    // --------------------------------------------------
    // Set up RELCAT in the relation cache
    // --------------------------------------------------

    RecBuffer relCatBlock(RELCAT_BLOCK);

    Attribute relCatRecord[RELCAT_NO_ATTRS];

    // Read RELCAT's own record from the relation catalog.
    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_RELCAT
    );


    // Create a temporary RelCacheEntry.
    RelCacheEntry relCacheEntry;


    // Convert Attribute[] into RelCatEntry.
    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );


    // Store the location of the catalog record.
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;


    // Allocate permanent memory for the cache entry.
    RelCacheTable::relCache[RELCAT_RELID] =
        (RelCacheEntry*)malloc(sizeof(RelCacheEntry));


    // Copy the temporary entry into the allocated memory.
    *(RelCacheTable::relCache[RELCAT_RELID]) =
        relCacheEntry;


    // --------------------------------------------------
    // Set up ATTRCAT in the relation cache
    // --------------------------------------------------

    Attribute attrCatRelRecord[RELCAT_NO_ATTRS];

    // Read ATTRCAT's record from RELCAT.
    relCatBlock.getRecord(
        attrCatRelRecord,
        RELCAT_SLOTNUM_FOR_ATTRCAT
    );


    // Create temporary cache entry.
    RelCacheEntry attrCatRelCacheEntry;


    // Convert the raw record to RelCatEntry.
    RelCacheTable::recordToRelCatEntry(
        attrCatRelRecord,
        &attrCatRelCacheEntry.relCatEntry
    );


    // Store where the record came from.
    attrCatRelCacheEntry.recId.block = RELCAT_BLOCK;
    attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;


    // Allocate memory.
    RelCacheTable::relCache[ATTRCAT_RELID] =
        (RelCacheEntry*)malloc(sizeof(RelCacheEntry));


    // Copy into the allocated memory.
    *(RelCacheTable::relCache[ATTRCAT_RELID]) =
        attrCatRelCacheEntry;


    // ==================================================
    // 3. SET UP ATTRIBUTE CACHE
    // ==================================================


    RecBuffer attrCatBlock(ATTRCAT_BLOCK);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];


    // --------------------------------------------------
    // Set up attributes belonging to RELCAT
    // --------------------------------------------------

    AttrCacheEntry* head = nullptr;
    AttrCacheEntry* tail = nullptr;


    for (int i = 0; i < RELCAT_NO_ATTRS; i++) {

        // Read attribute catalog record.
        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );


        // Allocate a new cache entry.
        AttrCacheEntry* newEntry =
            (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));


        // Convert Attribute[] into AttrCatEntry.
        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );


        // Store the record location.
        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = i;


        // Initially the new entry points to nothing.
        newEntry->next = nullptr;


        // First entry in the linked list.
        if (head == nullptr) {

            head = newEntry;
            tail = newEntry;

        }
        else {

            // Attach new entry to the end.
            tail->next = newEntry;

            // Move tail to the new entry.
            tail = newEntry;
        }
    }


    // Store the head of the linked list.
    AttrCacheTable::attrCache[RELCAT_RELID] = head;


    // --------------------------------------------------
    // Set up attributes belonging to ATTRCAT
    // --------------------------------------------------

    head = nullptr;
    tail = nullptr;


    for (
        int i = RELCAT_NO_ATTRS;
        i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS;
        i++
    ) {

        // Read attribute catalog record.
        attrCatBlock.getRecord(
            attrCatRecord,
            i
        );


        // Allocate new cache entry.
        AttrCacheEntry* newEntry =
            (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));


        // Convert raw record to AttrCatEntry.
        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );


        // Store record location.
        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = i;


        // Last entry currently.
        newEntry->next = nullptr;


        // First node?
        if (head == nullptr) {

            head = newEntry;
            tail = newEntry;

        }
        else {

            // Connect previous node to new node.
            tail->next = newEntry;

            // Move tail.
            tail = newEntry;
        }
    }


    // Store head of ATTRCAT's attribute list.
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;
}


// --------------------------------------------------
// Destructor
// --------------------------------------------------

OpenRelTable::~OpenRelTable() {

    // --------------------------------------------------
    // Free relation cache entries.
    // --------------------------------------------------

    for (int i = 0; i < MAX_OPEN; i++) {

        if (RelCacheTable::relCache[i] != nullptr) {

            free(RelCacheTable::relCache[i]);

            RelCacheTable::relCache[i] = nullptr;
        }
    }


    // --------------------------------------------------
    // Free attribute cache linked lists.
    // --------------------------------------------------

    for (int i = 0; i < MAX_OPEN; i++) {

        AttrCacheEntry* current =
            AttrCacheTable::attrCache[i];


        while (current != nullptr) {

            AttrCacheEntry* next =
                current->next;

            free(current);

            current = next;
        }


        AttrCacheTable::attrCache[i] = nullptr;
    }
}