#include "Disk_Class/Disk.h"
#include "Cache/OpenRelTable.h"
#include "Cache/RelCacheTable.h"
#include "Cache/AttrCacheTable.h"

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {

    // Create the disk object.
    Disk disk_run;

    // Create the static buffer.
    StaticBuffer buffer;

    // Create the Open Relation Table.
    // This initializes the relation and attribute caches.
    OpenRelTable cache;


    /*
     * We want to examine:
     *
     * RELCAT_RELID  -> Relation Catalog
     * ATTRCAT_RELID -> Attribute Catalog
     */

    for (int i = 0; i <= 1; i++) {

        // Structure to store relation catalog information.
        RelCatEntry relCatEntry;

        // Get relation information from RelCacheTable.
        int status =
            RelCacheTable::getRelCatEntry(i, &relCatEntry);

        // Check whether getting the relation information succeeded.
        if (status != SUCCESS) {
            cout << "Error getting relation catalog entry for relId "<< i << endl;

            continue;
        }

        // Print the relation name.
        cout << "Relation: "<< relCatEntry.relName<< endl;


        /*
         * Now get every attribute of this relation.
         *
         * If numAttrs = 4,
         * offsets will be:
         *
         * 0
         * 1
         * 2
         * 3
         */

        for (int j = 0; j < relCatEntry.numAttrs; j++) {

            // Structure to store attribute information.
            AttrCatEntry attrCatEntry;

            // Get the attribute information from AttrCacheTable.
            status =AttrCacheTable::getAttrCatEntry(i,j,&attrCatEntry);

            // Check whether getting the attribute succeeded.
            if (status != SUCCESS) {

                cout << "Error getting attribute "<< j<< " for relation "<< i<< endl;
                continue;
            }


            // Print attribute name and type.
            const char *attrType;

            if (attrCatEntry.attrType == NUMBER) {
                attrType = "NUM";
            }
            else {
                attrType = "STR";
            }

            cout << "  "<< attrCatEntry.attrName<< ": "<< attrType << endl;
        }

        cout << endl;
    }


    return 0;
}


