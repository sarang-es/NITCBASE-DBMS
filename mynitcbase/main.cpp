#include "Disk_Class/Disk.h"
#include "Buffer/BlockBuffer.h"
#include <cstring>
#include <iostream>
using namespace std;

int main(int argc, char *argv[]) {

    Disk disk_run;

    // Create objects for Relation Catalog and Attribute Catalog
    RecBuffer relCatBuffer(RELCAT_BLOCK);
    RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

    HeadInfo relCatHeader;
    HeadInfo attrCatHeader;

    // Load headers
    relCatBuffer.getHeader(&relCatHeader);
    attrCatBuffer.getHeader(&attrCatHeader);

    // Traverse all relations in RELCAT
    for (int i = 0; i < relCatHeader.numEntries; i++) {

        Attribute relCatRecord[RELCAT_NO_ATTRS];

        relCatBuffer.getRecord(relCatRecord, i);

        cout<<"Relation: "<<relCatRecord[RELCAT_REL_NAME_INDEX].sVal<<endl;

        int currentAttrBlock = ATTRCAT_BLOCK;

        while(currentAttrBlock != -1)
        {
            RecBuffer attrCatBuffer(currentAttrBlock);

            HeadInfo attrHeader;
            attrCatBuffer.getHeader(&attrHeader);

            for(int j = 0; j < attrHeader.numEntries; j++)
            {
                Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

                attrCatBuffer.getRecord(attrCatRecord, j);

                if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0)
                {
                    const char *attrType =attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal== NUMBER ? "NUM" : "STR";

                    cout<<"  "<<attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal<<": "<<attrType<<endl;
                }
            }

            currentAttrBlock = attrHeader.rblock;
        }

        cout<<endl;
    }

    return 0;
}