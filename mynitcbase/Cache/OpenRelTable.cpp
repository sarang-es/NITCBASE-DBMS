#include "OpenRelTable.h"
#include "AttrCacheTable.h"
#include "RelCacheTable.h"
#include <cstring>
#include <stdlib.h>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];
 
OpenRelTable::OpenRelTable() {

    // initialise everything to null
    for(int i=0; i < MAX_OPEN; i++) {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
        OpenRelTable::tableMetaInfo[i].free = true;
    }


    /************ Setting up Relation Cache entries ************/
    // (we need to populate relation cache with entries for the relation catalog and attribute catalog)

    /**** setting up Relation Catalog relation in the Relation Cache Table****/

    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

    // Use new instead of malloc
    RelCacheEntry *relCacheEntry  = new RelCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry->relCatEntry);
    // additional metadata
    relCacheEntry->recId.block = RELCAT_BLOCK;
    relCacheEntry->recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;
    RelCacheTable::relCache[RELCAT_RELID] = relCacheEntry;


    /**** setting up Attribute Catalog relation in the Relation Cache Table ****/
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

    RelCacheEntry *relCacheEntry2 = new RelCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry2->relCatEntry);
    // additional metadata
    relCacheEntry2->recId.block = RELCAT_BLOCK;
    relCacheEntry2->recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;
    RelCacheTable::relCache[ATTRCAT_RELID] = relCacheEntry2;

    
    /*********setting up tableMetaInfor for relation cache*******/
    tableMetaInfo[RELCAT_RELID].free = false;
    strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);



    /************ Setting up Attribute cache entries ************/
    // (we need to populate attribute cache with entries for the relation catalog
    //  and attribute catalog.)

    /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *tail = nullptr;
    // first 6 entries are attribtues of relation catalog. make it a linked list and add its head to attrCache
    for(int i=0; i < RELCAT_NO_ATTRS; i++) {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheEntry *curr = new AttrCacheEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
        curr->recId.block = ATTRCAT_BLOCK;
        curr->recId.slot = i;
        curr->next = nullptr;

        if(head==nullptr) {
            head = curr;
            tail = curr;
        } else {
            tail->next = curr;
            tail = tail->next;
        }

    }

    AttrCacheTable::attrCache[RELCAT_RELID] = head;

    /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
    head = nullptr;
    tail = nullptr;
    for(int i=RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i++) {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheEntry *curr = new AttrCacheEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &curr->attrCatEntry);
        curr->recId.block = ATTRCAT_BLOCK;
        curr->recId.slot = i;
        curr->next = nullptr;

        if(head==nullptr) {
            head = curr;
            tail =curr;
        } else {
            tail->next = curr;
            tail = tail->next;
        }
    }
    
    AttrCacheTable::attrCache[ATTRCAT_RELID] = head;

    /*********setting up tableMetaInfo for attribute cache*******/
    tableMetaInfo[ATTRCAT_RELID].free = false;
    strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable() {
    
    // close all open relations
    for(int i=2; i < MAX_OPEN; i++) {
        if(!tableMetaInfo[i].free) {
            OpenRelTable::closeRel(i);
        }
    }

    // Loop is safe because only thing that differs is RELID. note that RELCAT_NO_ATTRS = ATTRCAT_NO_ATTRS = 6
    for(int i=RELCAT_RELID; i<=ATTRCAT_RELID; i++) {

            if(RelCacheTable::relCache[i]->dirty) {

            RelCatEntry relCatEntry;
            RelCacheTable::getRelCatEntry(i, &relCatEntry);

            Attribute record[6];
            RelCacheTable::relCatEntryToRecord(&relCatEntry, record);

            RecId recId = RelCacheTable::relCache[i]->recId;
            RecBuffer relCatBlock(recId.block);

            //write back to buffer
            relCatBlock.setRecord(record, recId.slot);
        }

        //free the dynamic memory allocated for this RelCacheEntry
        free(RelCacheTable::relCache[i]);
    }

    // I can use the same loop for attribute cache table
    for(int i=RELCAT_RELID; i<=ATTRCAT_RELID; i++) {

        AttrCacheEntry *curr = AttrCacheTable::attrCache[i];
        AttrCacheEntry *next = nullptr;

        // There will be 6 nodes in linked list
        for(int index=0; index < 6; index++) {

            if(curr->dirty) {
                AttrCatEntry attrCatEntry;
                AttrCacheTable::getAttrCatEntry(i, index, &attrCatEntry);

                Attribute record[6];
                AttrCacheTable::attrCatEntryToRecord(&attrCatEntry, record);

                RecId recId = curr->recId;
                RecBuffer attrCatBlock(recId.block);

                attrCatBlock.setRecord(record, recId.block);
            }
            next = curr->next;
            free(curr);
            curr = next;
        }
    }
}

int OpenRelTable::getFreeOpenRelTableEntry() {
    // traverse through tableMetaInfo array and find a free entry - that is the relId - return it
    for(int i=0; i < MAX_OPEN; i++) {
        if(tableMetaInfo[i].free) {
            return i;
        }
    }
    // if maximum number of tables are open, return error
    return E_CACHEFULL;
} 

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
    // traverse through tableMetaInfo arrary and find the entry with the given relation name - return its id
    for(int i=0; i < MAX_OPEN; i++) {
        if(!tableMetaInfo[i].free && strcmp(relName, tableMetaInfo[i].relName)==0) {
            return i;
        }
    }
    // else relation is not open
    return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
    /*********Setting up rel cache table *********/

    // what if the relation is already open?
    int ret = getRelId(relName);
    if(ret != E_RELNOTOPEN) {
        return ret;
    }

    // if it's not open, I open it
    // but i need a new relId
    int relId = getFreeOpenRelTableEntry();
    if(relId == E_CACHEFULL) {
        return E_CACHEFULL;
    }

    Attribute attrVal;
    strcpy(attrVal.sVal, relName);

    // ISO C++ forbids converting a string constant to 'char*', so manually assign it to char*
    char relationName[ATTR_SIZE];
    strcpy(relationName, RELCAT_ATTR_RELNAME);

    // reset search Index of relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relationName, attrVal, EQ);

    if(recId.block == -1 && recId.slot == -1) {
        return E_RELNOTEXIST;
    }

    // now i have block mumber and slot number of metadata of Student relation
    RecBuffer buffer(recId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    buffer.getRecord(relCatRecord, recId.slot);

    struct RelCacheEntry *relCacheEntry = new RelCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry->relCatEntry);
    relCacheEntry->recId = recId;
    relCacheEntry->dirty = false;

    // insert the newly constructed RelCacheEntry to RelCacheTable
    RelCacheTable::relCache[relId] = relCacheEntry;


    /*******Setting up attribute cache entry **********/

    AttrCacheEntry* listHead = nullptr;
    AttrCacheEntry* tail = nullptr;
    strcpy(relationName, ATTRCAT_ATTR_RELNAME);

    // Reset search index before searching ATTRCAT
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    while(true) {
        // scan the entire attribute catalog using linearSearch 

        RecId attrCatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relationName, attrVal, EQ);

        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1) {
            break;
        }

        RecBuffer attrCatBuffer(attrCatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

        struct AttrCacheEntry *attrCacheEntry = new AttrCacheEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId = attrCatRecId;
        attrCacheEntry->next = nullptr;
        attrCacheEntry->dirty = false;

        // add it to the linked list
        if(listHead==nullptr && tail==nullptr) {
            listHead = attrCacheEntry;
            tail = attrCacheEntry;
        } else {
            tail->next = attrCacheEntry;
            tail = tail->next;
        }

    }

    AttrCacheTable::attrCache[relId] = listHead;

    /****** Setting up metadata in the Open Relation Table for the relation******/

    tableMetaInfo[relId].free = false;
    strcpy(tableMetaInfo[relId].relName, relName);

    return relId;

}


int OpenRelTable::closeRel(int relId) {
    // rel cat and attr cat cannot be closed
    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }

    // if relid is outside range
    if(relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if(tableMetaInfo[relId].free == true) {
        return E_RELNOTOPEN;
    }

    // write back if dirty
    if (RelCacheTable::relCache[relId] != nullptr && RelCacheTable::relCache[relId]->dirty) {
        // Get the Relation Catalog entry from relCache
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        RecId recId = RelCacheTable::relCache[relId]->recId;

        // Convert it to a record using RelCacheTable::relCatEntryToRecord()
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&relCatEntry, record);

        // Write back to the buffer
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(record, recId.slot);
    }
    
    // release the memory allocated in rel cat for this relation
    if(RelCacheTable::relCache[relId] != nullptr) {
        delete RelCacheTable::relCache[relId];
        RelCacheTable::relCache[relId] = nullptr;
    }


    // release memory allocated for each node in the attr cache linked list
    AttrCacheEntry *current = AttrCacheTable::attrCache[relId];
    AttrCacheEntry *next = nullptr;
    
    while(current) {
        next = current->next;
        delete current;
        current = next;
    }

    AttrCacheTable::attrCache[relId] = nullptr;
    
    // this relId can be used for other relations in future, so free it
    tableMetaInfo[relId].free = true;

    return SUCCESS;
}