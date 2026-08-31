#include "BlockBuffer.h"
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum){
    this->blockNum=blockNum;
}

RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {
}

int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;

    //Load the block into the static buffer and get a pointer to it
    int status = loadBlockAndGetBufferPtr(&bufferPtr);

    if(status!= SUCCESS){
        return status;
    }

    //Extract the header field from the block
    memcpy(&head->blockType,  bufferPtr + 0,  4);
    memcpy(&head->pblock,    bufferPtr + 4,  4);
    memcpy(&head->lblock,    bufferPtr + 8,  4);
    memcpy(&head->rblock,    bufferPtr + 12, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->numAttrs,   bufferPtr + 20, 4);
    memcpy(&head->numSlots,   bufferPtr + 24, 4);
    memcpy(head->reserved,    bufferPtr + 28, 4);

    return SUCCESS;

}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;

    //Get the number of attributes and slots from the block header
    int status = this->getHeader(&head);

    if(status!= SUCCESS){
        return status;
    }

    unsigned char *bufferPtr;

    //Load the block into the static buffer and get a pointer to it
    status = loadBlockAndGetBufferPtr(&bufferPtr);

    if(status!= SUCCESS){
        return status;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    //size of one record
    int recordSize = attrCount * ATTR_SIZE;

    /*
     * Record area begins after:
     * 1. the block header
     * 2. the slot map
     *
     * Then move forward by slotNum records.
     */
    unsigned char *slotPointer =
        bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

    //Copy the complete record into the caller's array
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;

    //Get the Block Number
    int status = this->getHeader(&head);

    if(status!= SUCCESS){
        return status;
    }

    unsigned char *bufferPtr;

    //Load the block into the static buffer and get a pointer to it
    status = loadBlockAndGetBufferPtr(&bufferPtr);

    if(status!= SUCCESS){
        return status;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;
    int recordSize = attrCount * ATTR_SIZE;

    //find the location of the record in the block
    unsigned char *slotPointer =
        bufferPtr +
        HEADER_SIZE +
        slotCount +
        (recordSize * slotNum);

    //Copy the record into the block
    memcpy(slotPointer, rec, recordSize);

    return SUCCESS;

}

int RecBuffer::getSlotMap(unsigned char *slotMap) {

    unsigned char *bufferPtr;

    // Load block into static buffer
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if(ret != SUCCESS) {
        return ret;
    }

    HeadInfo head;

    // Read block header
    ret = getHeader(&head);

    if(ret != SUCCESS) {
        return ret;
    }

    int slotCount = head.numSlots;

    // Slot map begins immediately after header
    unsigned char *slotMapInBuffer =
        bufferPtr + HEADER_SIZE;

    // Copy slot map into caller's array
    memcpy(
        slotMap,
        slotMapInBuffer,
        slotCount
    );

    return SUCCESS;
}

// make comparing easy for different attribute types
int compareAttrs(union Attribute attr1,union Attribute attr2,int attrType) {

    double diff;

    if (attrType == STRING) {
        diff = strcmp(attr1.sVal,attr2.sVal);
    }
    else {
        diff = attr1.nVal - attr2.nVal;
    }
    if (diff > 0) {
        return 1;
    }
    if (diff < 0) {
        return -1;
    }

    return 0;
}


int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
    
    // check if the block is already in the static buffer
    int bufferNum=StaticBuffer::getBufferNum(this->blockNum);

    // Block is not currently in the static buffer, so we need to load it
    if(bufferNum==E_BLOCKNOTINBUFFER){

        // get a free buffer slot
        bufferNum=StaticBuffer::getFreeBuffer(this->blockNum);

        if(bufferNum==E_OUTOFBOUND)
            return E_OUTOFBOUND;

        // Read disk Block into selected buffer slot
        int status=Disk::readBlock(StaticBuffer::blocks[bufferNum],this->blockNum);

        if(status!=SUCCESS)
            return status;
    }

    // Set the buffer pointer to the selected buffer slot
    *buffPtr=StaticBuffer::blocks[bufferNum];
    return SUCCESS;
}