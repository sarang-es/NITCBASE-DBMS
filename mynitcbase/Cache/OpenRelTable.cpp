#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>

const int STUDENTS_RELID = 2;

OpenRelTable::OpenRelTable()
{
    /*
     * Initially no relation is loaded into the caches.
     *
     * relCache[i]  stores relation information.
     * attrCache[i] stores the attribute list of that relation.
     */
    for (int i = 0; i < MAX_OPEN; ++i)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }


    /*
     * RELCAT_BLOCK is the disk block containing
     * the Relation Catalog.
     */
    RecBuffer relCatBlock(RELCAT_BLOCK);


    /*
     * Create space to hold one record from RELATIONCAT.
     *
     * A Relation Catalog record contains:
     * relation name, number of attributes, number of records,
     * first block, last block and number of slots per block.
     */
    Attribute relCatRecord[RELCAT_NO_ATTRS];


    /*
     * Read the record that describes RELATIONCAT itself.
     *
     * RELCAT_SLOTNUM_FOR_RELCAT is the slot number
     * where RELATIONCAT's own catalog entry is stored.
     */
    relCatBlock.getRecord(
        relCatRecord,
        RELCAT_SLOTNUM_FOR_RELCAT
    );


    /*
     * Create a temporary relation-cache entry.
     * This contains the actual Relation Catalog information
     * along with extra cache information such as recId.
     */
    struct RelCacheEntry relCacheEntry;


    /*
     * Convert the raw Attribute[] record obtained from disk
     * into the easier-to-use RelCatEntry structure.
     */
    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &relCacheEntry.relCatEntry
    );


    /*
     * Remember where this Relation Catalog record
     * physically exists on disk.
     */
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    relCacheEntry.dirty = false;
    relCacheEntry.searchIndex = {-1, -1};


    /*
     * Allocate permanent memory for the RELATIONCAT cache entry.
     *
     * RELCAT_RELID is 0, so this effectively creates:
     *
     * relCache[0] -> RELATIONCAT
     */
    RelCacheTable::relCache[RELCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));


    /*
     * Copy the prepared RELATIONCAT information
     * into the newly allocated cache entry.
     */
    *(RelCacheTable::relCache[RELCAT_RELID]) =
        relCacheEntry;


        // Read the RELATIONCAT record that describes ATTRIBUTECAT.
    Attribute attrCatRelRecord[RELCAT_NO_ATTRS];

    relCatBlock.getRecord(
        attrCatRelRecord,
        RELCAT_SLOTNUM_FOR_ATTRCAT
    );

    struct RelCacheEntry attrCatRelCacheEntry;

    // Convert the raw catalog record into a RelCatEntry.
    RelCacheTable::recordToRelCatEntry(
        attrCatRelRecord,
        &attrCatRelCacheEntry.relCatEntry
    );

    // Store where this record exists in RELATIONCAT.
    attrCatRelCacheEntry.recId.block = RELCAT_BLOCK;
    attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    attrCatRelCacheEntry.dirty = false;
    attrCatRelCacheEntry.searchIndex = {-1, -1};

    // Store ATTRIBUTECAT in relation cache position 1.
    RelCacheTable::relCache[ATTRCAT_RELID] =
        (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));

    *(RelCacheTable::relCache[ATTRCAT_RELID]) =
        attrCatRelCacheEntry;


     // Attribute Catalog block contains the attribute descriptions.
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *prev = nullptr;

    // Slots 0-5 describe the six attributes of RELATIONCAT.
    for (int i = 0; i < RELCAT_NO_ATTRS; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry *newEntry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = i;

        newEntry->dirty = false;
        newEntry->searchIndex = {-1, -1};

        newEntry->next = nullptr;
        if (head == nullptr)
        {
            head = newEntry;
        }
        else
        {
            prev->next = newEntry;
        }

        prev = newEntry;
    }

    // attrCache[0] points to RELATIONCAT's attribute linked list.
    AttrCacheTable::attrCache[RELCAT_RELID] = head;   


    AttrCacheEntry *attrCatHead = nullptr;
    AttrCacheEntry *attrCatPrev = nullptr;

    // Slots 6-11 describe the six attributes of ATTRIBUTECAT.
    for (int i = 0; i < ATTRCAT_NO_ATTRS; i++)
    {
        int slotNum = RELCAT_NO_ATTRS + i;

        attrCatBlock.getRecord(attrCatRecord, slotNum);

        AttrCacheEntry *newEntry =
            (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

        AttrCacheTable::recordToAttrCatEntry(
            attrCatRecord,
            &newEntry->attrCatEntry
        );

        newEntry->recId.block = ATTRCAT_BLOCK;
        newEntry->recId.slot = slotNum;

        newEntry->dirty = false;
        newEntry->searchIndex = {-1, -1};

        newEntry->next = nullptr;

        if (attrCatHead == nullptr)
        {
            attrCatHead = newEntry;
        }
        else
        {
            attrCatPrev->next = newEntry;
        }

        attrCatPrev = newEntry;
    }

    // attrCache[1] points to ATTRIBUTECAT's attribute list.
    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCatHead;

    // stage 3 modification
    

    HeadInfo relCatHeader;
    relCatBlock.getHeader(&relCatHeader);

    // Find the Students record inside RELATIONCAT.
    for (int i = 0; i < relCatHeader.numEntries; i++)
    {
        Attribute record[RELCAT_NO_ATTRS];
        relCatBlock.getRecord(record, i);

        if (strcmp(
                record[RELCAT_REL_NAME_INDEX].sVal,
                "Students"
            ) == 0)
        {
            RelCacheEntry studentEntry;

            RelCacheTable::recordToRelCatEntry(
                record,
                &studentEntry.relCatEntry
            );

            studentEntry.recId.block = RELCAT_BLOCK;
            studentEntry.recId.slot = i;

            RelCacheTable::relCache[STUDENTS_RELID] =
                (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

            *(RelCacheTable::relCache[STUDENTS_RELID]) =
                studentEntry;

            break;
        }
    }
    //modification for stage 3
    //for attrcat, find the attributes of Students in ATTRIBUTECAT
    AttrCacheEntry *studentHead = nullptr;
    AttrCacheEntry *studentPrev = nullptr;

    int attrBlockNum = ATTRCAT_BLOCK;

    // Search Attribute Catalog for attributes belonging to Students.
    while (attrBlockNum != -1)
    {
        RecBuffer block(attrBlockNum);

        HeadInfo header;
        block.getHeader(&header);

        for (int i = 0; i < header.numEntries; i++)
        {
            Attribute record[ATTRCAT_NO_ATTRS];
            block.getRecord(record, i);

            if (strcmp(
                    record[ATTRCAT_REL_NAME_INDEX].sVal,
                    "Students"
                ) == 0)
            {
                AttrCacheEntry *newEntry =
                    (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

                AttrCacheTable::recordToAttrCatEntry(
                    record,
                    &newEntry->attrCatEntry
                );

                newEntry->recId.block = attrBlockNum;
                newEntry->recId.slot = i;

                newEntry->dirty = false;
                newEntry->searchIndex = {-1, -1};

                newEntry->next = nullptr;
                
                if (studentHead == nullptr)
                    studentHead = newEntry;
                else
                    studentPrev->next = newEntry;

                studentPrev = newEntry;
            }
        }

        // Move to the next Attribute Catalog block.
        attrBlockNum = header.rblock;
}

AttrCacheTable::attrCache[STUDENTS_RELID] = studentHead;
}

OpenRelTable::~OpenRelTable()
{
    for (int relId = 0; relId < MAX_OPEN; relId++)
    {
        // Free the linked list of attribute-cache entries.
        AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];

        while (entry != nullptr)
        {
            AttrCacheEntry *nextEntry = entry->next;
            free(entry);
            entry = nextEntry;
        }

        AttrCacheTable::attrCache[relId] = nullptr;

        // Free the relation-cache entry.
        if (RelCacheTable::relCache[relId] != nullptr)
        {
            free(RelCacheTable::relCache[relId]);
            RelCacheTable::relCache[relId] = nullptr;
        }
    }
}


int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
    if (strcmp(relName, RELCAT_RELNAME) == 0)
    {
        return RELCAT_RELID;
    }

    if (strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return ATTRCAT_RELID;
    }

    if (strcmp(relName, "Students") == 0)
    {
        return STUDENTS_RELID;
    }

    return E_RELNOTOPEN;
}