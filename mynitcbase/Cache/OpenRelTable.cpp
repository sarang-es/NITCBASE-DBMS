#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

int OpenRelTable::openRel(char relName[ATTR_SIZE])
{
    // Check if the relation is already open
    int relId = getRelId(relName);
    if(relId >= 0)
    {
        return relId; // Relation is already open return the RelId
    }

    // Get a free entry in the open relation table
    relId = getFreeOpenRelTableEntry();
    if(relId < 0)
    {
        return E_CACHEFULL; // No free entry available
    }

    // Open the Relation Catalog to get the relation's metadata
    RecBuffer relCatBlock(RELCAT_BLOCK);

    HeadInfo relCatHeader;

    int ret = relCatBlock.getHeader(&relCatHeader);

    if(ret!= SUCCESS)
    {
        return ret; // Error getting header
    }

    // To store one RELATIONCAT record
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    bool found = false;
    int relCatSlot = -1;

    // Search for the relation in the Relation Catalog
    for(int i=0; i<relCatHeader.numEntries; i++)
    {
        ret = relCatBlock.getRecord(relCatRecord, i);
        if(ret != SUCCESS)
        {
            return ret; // Error getting record
        }

        // Check if the relation name matches
        if(strcmp(relCatRecord[0].sVal, relName) == 0)
        {
            found = true;
            relCatSlot = i;
            break;
        }
    }
    
        if(!found)
        {
            return E_RELNOTEXIST; // Relation does not exist
        }

        // Covert the RELATIONCAT record to RelCatEntry
        RelCacheEntry relCacheEntry;
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
        
        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = relCatSlot;

        relCacheEntry.dirty = false;
        relCacheEntry.searchIndex = {-1, -1};

        // Allocate memory for the new relation cache entry
        RelCacheTable::relCache[relId] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
        if(RelCacheTable::relCache[relId] == nullptr)
        {
            return E_CACHEFULL; // Memory allocation failed
        }

        // Copy the prepared RelCacheEntry into the allocated memory
        *(RelCacheTable::relCache[relId]) = relCacheEntry;

        // Construct the attribute cache linked list for the relation
        AttrCacheEntry *head = nullptr;
        AttrCacheEntry *tail = nullptr;

        int attrCatBlockNum = ATTRCAT_BLOCK;

        while(attrCatBlockNum != -1)
        {
            RecBuffer attrCatBlock(attrCatBlockNum);
            HeadInfo attrCatHeader;

            ret = attrCatBlock.getHeader(&attrCatHeader);
            if(ret != SUCCESS)
            {
                return ret; // Error getting header
            }

            for(int i=0; i<attrCatHeader.numEntries; i++)
            {
                Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
                ret = attrCatBlock.getRecord(attrCatRecord, i);
                if(ret != SUCCESS)
                {
                    return ret; // Error getting record
                }

                // Check if the attribute belongs to the relation
                if(strcmp(attrCatRecord[0].sVal, relName) == 0)
                {
                    AttrCacheEntry *newEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
                    if(newEntry == nullptr)
                    {
                        return E_CACHEFULL; // Memory allocation failed
                    }

                    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &newEntry->attrCatEntry);
                    newEntry->recId.block = attrCatBlockNum;
                    newEntry->recId.slot = i;
                    newEntry->dirty = false;
                    newEntry->searchIndex = {-1, -1};
                    newEntry->next = nullptr;

                    if(head == nullptr)
                    {
                        head = newEntry;
                        tail = newEntry;
                    }
                    else
                    {
                        tail->next = newEntry;
                        tail = newEntry;
                    }
                }
            }

            // Move to the next attribute catalog block
            attrCatBlockNum = attrCatHeader.rblock;
        }

        // Store the attribute cache linked list in the attribute cache table
        AttrCacheTable::attrCache[relId] = head;

        // Mark the relation as open in the open relation table
        tableMetaInfo[relId].free = false;
        strcpy(tableMetaInfo[relId].relName, relName);
        
        return relId; 
}

int OpenRelTable::closeRel(int relId)
{
    // Check whether relId is within valid range.
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    // RELCAT and ATTRCAT cannot be closed.
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    {
        return E_NOTPERMITTED;
    }

    // Check whether the relation is actually open.
    if (tableMetaInfo[relId].free)
    {
        return E_RELNOTOPEN;
    }

    // Free the relation cache entry.
    if (RelCacheTable::relCache[relId] != nullptr)
    {
        free(RelCacheTable::relCache[relId]);
        RelCacheTable::relCache[relId] = nullptr;
    }

    // Free the attribute cache linked list.
    AttrCacheEntry *current =
        AttrCacheTable::attrCache[relId];

    while (current != nullptr)
    {
        AttrCacheEntry *next = current->next;

        free(current);

        current = next;
    }

    AttrCacheTable::attrCache[relId] = nullptr;

    // Mark this OpenRelTable entry as free.
    tableMetaInfo[relId].free = true;

    // Clear the relation name.
    tableMetaInfo[relId].relName[0] = '\0';

    return SUCCESS;
}
   
OpenRelTable::OpenRelTable()
{
    //  Initially no relation is loaded into the caches.
    //  relCache[i]  stores relation information.
    //  attrCache[i] stores the attribute list of that relation.
    for (int i = 0; i < MAX_OPEN; ++i)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;

        tableMetaInfo[i].free = true;
    }

    /* RELCAT_BLOCK is the disk block containing
     * the Relation Catalog.*/
    RecBuffer relCatBlock(RELCAT_BLOCK);


    /*
     * Create space to hold one record from RELATIONCAT.
     * A Relation Catalog record contains:
     * relation name, number of attributes, number of records,
     * first block, last block and number of slots per block.
     */
    Attribute relCatRecord[RELCAT_NO_ATTRS];


    /*
     * Read the record that describes RELATIONCAT itself.
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
     * RELCAT_RELID is 0, so this effectively creates:
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

    // Mark RELCAT and ATTRCAT as occupied in tableMetaInfo

    tableMetaInfo[RELCAT_RELID].free = false;
    strcpy(tableMetaInfo[RELCAT_RELID].relName,RELCAT_RELNAME);

    tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(tableMetaInfo[ATTRCAT_RELID].relName,ATTRCAT_RELNAME);


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
}

OpenRelTable::~OpenRelTable()
{
    // Close dynamically opened relations.
    for (int relId = 2; relId < MAX_OPEN; relId++)
    {
        if (!tableMetaInfo[relId].free)
        {
            OpenRelTable::closeRel(relId);
        }
    }

    // Free RELCAT and ATTRIBUTECAT.
    for (int relId = 0; relId < 2; relId++)
    {
        if (AttrCacheTable::attrCache[relId] != nullptr)
        {
            AttrCacheEntry *entry =
                AttrCacheTable::attrCache[relId];

            while (entry != nullptr)
            {
                AttrCacheEntry *next = entry->next;
                free(entry);
                entry = next;
            }

            AttrCacheTable::attrCache[relId] = nullptr;
        }

        if (RelCacheTable::relCache[relId] != nullptr)
        {
            free(RelCacheTable::relCache[relId]);
            RelCacheTable::relCache[relId] = nullptr;
        }
    }
}


int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
    for (int i = 0; i < MAX_OPEN; i++)
    {
        if (!tableMetaInfo[i].free &&
            strcmp(tableMetaInfo[i].relName, relName) == 0)
        {
            return i;
        }
    }

    return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry()
{
    for (int i = 2; i < MAX_OPEN; i++)
    {
        if (tableMetaInfo[i].free)
        {
            return i;
        }
    }

    return E_CACHEFULL;
}