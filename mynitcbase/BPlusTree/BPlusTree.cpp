#include "BPlusTree.h"

#include <cstring>

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
    // System catalogs are managed by the database and cannot be indexed.
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
        return E_NOTPERMITTED;
    }

    // Find the attribute metadata and skip work if its index already exists.
    AttrCatEntry attr;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attr);
    if (ret != SUCCESS) {
        return ret;
    }

    if (attr.rootBlock != -1) {
        return SUCCESS;
    }

    // Start with an empty leaf. Later insertions may split it into more nodes.
    IndLeaf root;
    int rootNum = root.getBlockNum();
    if (rootNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo head{};
    head.blockType = IND_LEAF;
    head.pblock = head.lblock = head.rblock = -1;
    head.numEntries = 0;

    ret = root.setHeader(&head);
    if (ret != SUCCESS) {
        root.releaseBlock();
        return ret;
    }

    // Save the root in the attribute catalog cache before populating the tree.
    attr.rootBlock = rootNum;
    ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attr);
    if (ret != SUCCESS) {
        root.releaseBlock();
        return ret;
    }

    // Build the index for rows that were inserted before CREATE INDEX.
    RelCatEntry rel;
    ret = RelCacheTable::getRelCatEntry(relId, &rel);
    if (ret != SUCCESS) {
        return ret;
    }

    int block = rel.firstBlk;
    while (block != -1) {
        RecBuffer records(block);
        HeadInfo rh;
        ret = records.getHeader(&rh);
        if (ret != SUCCESS) {
            return ret;
        }

        unsigned char slots[rh.numSlots];
        ret = records.getSlotMap(slots);
        if (ret != SUCCESS) {
            return ret;
        }

        for (int slot = 0; slot < rh.numSlots; ++slot) {
            if (slots[slot] != SLOT_OCCUPIED) {
                continue;
            }

            Attribute record[rh.numAttrs];
            ret = records.getRecord(record, slot);
            if (ret != SUCCESS) {
                return ret;
            }

            // Insert the indexed attribute value with the row's disk location.
            ret = bPlusInsert(relId, attrName, record[attr.offset], RecId{block, slot});
            if (ret != SUCCESS) {
                AttrCatEntry current;
                AttrCacheTable::getAttrCatEntry(relId, attrName, &current);
                bPlusDestroy(current.rootBlock);
                current.rootBlock = -1;
                AttrCacheTable::setAttrCatEntry(relId, attrName, &current);
                return ret;
            }
        }
        block = rh.rblock;
    }
    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    // Reject invalid block numbers before reading their type.
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);
    if (type == IND_LEAF) {
        IndLeaf(rootBlockNum).releaseBlock();
        return SUCCESS;
    }

    if (type != IND_INTERNAL) {
        return E_INVALIDBLOCK;
    }

    // Recursively destroy every child, then release this internal node.
    IndInternal node(rootBlockNum);
    HeadInfo head;
    int ret = node.getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    if (head.numEntries > 0) {
        InternalEntry entry;
        ret = node.getEntry(&entry, 0);
        if (ret != SUCCESS) {
            return ret;
        }

        ret = bPlusDestroy(entry.lChild);
        if (ret != SUCCESS) {
            return ret;
        }

        for (int i = 0; i < head.numEntries; ++i) {
            ret = node.getEntry(&entry, i);
            if (ret != SUCCESS) {
                return ret;
            }

            ret = bPlusDestroy(entry.rChild);
            if (ret != SUCCESS) {
                return ret;
            }
        }
    }

    node.releaseBlock();
    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute value, int attrType) {
    int block = rootBlock;

    // Follow child pointers until we reach the leaf for this value.
    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
        IndInternal node(block);
        HeadInfo head;
        int ret = node.getHeader(&head);
        if (ret != SUCCESS) {
            return ret;
        }

        if (head.numEntries <= 0) {
            return E_INVALIDBLOCK;
        }

        InternalEntry entry;
        int i = 0;
        for (; i < head.numEntries; ++i) {
            ret = node.getEntry(&entry, i);
            if (ret != SUCCESS) {
                return ret;
            }

            if (compareAttrs(value, entry.attrVal, attrType) <= 0) break;
        }

        if (i == head.numEntries) {
            ret = node.getEntry(&entry, head.numEntries - 1);
            if (ret != SUCCESS) {
                return ret;
            }

            block = entry.rChild;
        } else {
            block = entry.lChild;
        }
    }

    return StaticBuffer::getStaticBlockType(block) == IND_LEAF ? block : E_INVALIDBLOCK;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recordId) {
    AttrCatEntry attr;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attr);
    if (ret != SUCCESS) {
        return ret;
    }

    if (attr.rootBlock == -1) {
        return E_NOINDEX;
    }

    // Find the destination leaf, then package the value and record id together.
    int leaf = findLeafToInsert(attr.rootBlock, attrVal, attr.attrType);
    if (leaf < 0) {
        return leaf;
    }

    Index item{};
    item.attrVal = attrVal;
    item.block = recordId.block;
    item.slot = recordId.slot;

    ret = insertIntoLeaf(relId, attrName, leaf, item);
    if (ret == E_DISKFULL) {
        // A partial index is unsafe; remove it and mark the attribute unindexed.
        bPlusDestroy(attr.rootBlock);
        attr.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attr);
    }

    return ret;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index item) {
    IndLeaf leaf(blockNum);
    HeadInfo head;
    int ret = leaf.getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    // Load every existing entry before shifting the array to make room.
    Index entries[MAX_KEYS_LEAF + 1];
    for (int i = 0; i < head.numEntries; ++i) {
        ret = leaf.getEntry(&entries[i], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    // Find the sorted insertion position (new duplicates go after existing ones).
    int pos = 0;
    while (pos < head.numEntries) {
        AttrCatEntry attr;
        ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attr);
        if (ret != SUCCESS) {
            return ret;
        }

        if (compareAttrs(item.attrVal, entries[pos].attrVal, attr.attrType) < 0) break;
        ++pos;
    }

    for (int i = head.numEntries; i > pos; --i) entries[i] = entries[i-1];
    entries[pos] = item;

    // If the leaf has room, write the updated entries back in place.
    if (head.numEntries < MAX_KEYS_LEAF) {
        ++head.numEntries;
        ret = leaf.setHeader(&head);
        if (ret != SUCCESS) {
            return ret;
        }

        for (int i = 0; i < head.numEntries; ++i) {
            ret = leaf.setEntry(&entries[i], i);
            if (ret != SUCCESS) {
                return ret;
            }
        }

        return SUCCESS;
    }

    // Otherwise split the full leaf and propagate its separator upward.
    int rightNum = splitLeaf(blockNum, entries);
    if (rightNum < 0) {
        return rightNum;
    }

    HeadInfo oldHead;
    leaf.getHeader(&oldHead);
    int parent = oldHead.pblock;
    if (parent == -1) {
        AttrCatEntry attr;
        ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attr);
        if (ret != SUCCESS) return ret;
        return createNewRoot(relId, attrName, entries[MIDDLE_INDEX_LEAF].attrVal, blockNum, rightNum);
    }
    InternalEntry separator{blockNum, entries[MIDDLE_INDEX_LEAF].attrVal, rightNum};
    return insertIntoInternal(relId, attrName, parent, separator);
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    // Allocate a new right leaf and read the current leaf's links.
    IndLeaf left(leafBlockNum), right;
    int rightNum = right.getBlockNum();
    if (rightNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo lh, rh;
    int ret = left.getHeader(&lh);
    if (ret != SUCCESS) {
        return ret;
    }

    // Link the new right leaf between the old leaf and its former right neighbor.
    rh = lh;
    rh.blockType = IND_LEAF;
    rh.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rh.lblock = leafBlockNum;
    rh.rblock = lh.rblock;
    ret = right.setHeader(&rh);
    if (ret != SUCCESS) {
        return ret;
    }

    lh.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    lh.rblock = rightNum;
    ret = left.setHeader(&lh);
    if (ret != SUCCESS) {
        return ret;
    }

    if (rh.rblock != -1) {
        IndLeaf next(rh.rblock);
        HeadInfo nh;
        ret = next.getHeader(&nh);
        if (ret != SUCCESS) {
            return ret;
        }

        nh.lblock = rightNum;
        ret = next.setHeader(&nh);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    // Keep the lower half in the left leaf and move the upper half right.
    for (int i = 0; i < lh.numEntries; ++i) {
        ret = left.setEntry(&indices[i], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    for (int i = 0; i < rh.numEntries; ++i) {
        ret = right.setEntry(&indices[i + lh.numEntries], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return rightNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute value, int left, int right) {
    IndInternal root;
    int rootNum = root.getBlockNum();
    if (rootNum == E_DISKFULL) {
        bPlusDestroy(right);
        return E_DISKFULL;
    }

    // A new root contains one separator and points to the two split children.
    HeadInfo head{};
    head.blockType = IND_INTERNAL;
    head.pblock = head.lblock = head.rblock = -1;
    head.numEntries = 1;

    int ret = root.setHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    InternalEntry entry{left, value, right};
    ret = root.setEntry(&entry, 0);
    if (ret != SUCCESS) {
        return ret;
    }

    // Point each child back to its new parent.
    int children[2] = {left, right};
    for (int child : children) {
        BlockBuffer childBlock(child);
        HeadInfo childHead;
        ret = childBlock.getHeader(&childHead);
        if (ret != SUCCESS) {
            return ret;
        }

        childHead.pblock = rootNum;
        ret = childBlock.setHeader(&childHead);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    // The attribute catalog stores the root used by later searches and inserts.
    AttrCatEntry attr;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attr);
    if (ret != SUCCESS) {
        return ret;
    }

    attr.rootBlock = rootNum;
    return AttrCacheTable::setAttrCatEntry(relId, attrName, &attr);
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int blockNum, InternalEntry added) {
    IndInternal node(blockNum);
    HeadInfo head;
    int ret = node.getHeader(&head);
    if (ret != SUCCESS) {
        return ret;
    }

    // Read all entries before shifting any of them in memory.
    InternalEntry all[MAX_KEYS_INTERNAL + 1];
    for (int i = 0; i < head.numEntries; ++i) {
        ret = node.getEntry(&all[i], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    // Find the sorted position for the separator coming from a child split.
    int pos = 0;
    while (pos < head.numEntries) {
        AttrCatEntry attr;
        ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attr);
        if (ret != SUCCESS) {
            return ret;
        }

        if (compareAttrs(added.attrVal, all[pos].attrVal, attr.attrType) < 0) break;
        ++pos;
    }

    // Open a gap, insert the separator, and reconnect the next entry's left child.
    for (int i = head.numEntries; i > pos; --i) all[i] = all[i-1];
    all[pos] = added;
    if (pos < head.numEntries) {
        all[pos + 1].lChild = added.rChild;
    }

    // If the node has room, save the expanded entry list.
    if (head.numEntries < MAX_KEYS_INTERNAL) {
        ++head.numEntries;
        ret = node.setHeader(&head);
        if (ret != SUCCESS) {
            return ret;
        }

        for (int i = 0; i < head.numEntries; ++i) {
            ret = node.setEntry(&all[i], i);
            if (ret != SUCCESS) {
                return ret;
            }
        }

        // The newly allocated right child now belongs to this internal node.
        BlockBuffer child(added.rChild);
        HeadInfo childHead;
        ret = child.getHeader(&childHead);
        if (ret != SUCCESS) {
            return ret;
        }

        childHead.pblock = blockNum;
        return child.setHeader(&childHead);
    }

    // Otherwise split this node and propagate its middle separator upward.
    int right = splitInternal(blockNum, all);
    if (right < 0) {
        return right;
    }

    HeadInfo current;
    ret = node.getHeader(&current);
    if (ret != SUCCESS) {
        return ret;
    }

    if (current.pblock == -1) {
        return createNewRoot(relId, attrName, all[MIDDLE_INDEX_INTERNAL].attrVal,
                             blockNum, right);
    }

    InternalEntry up{blockNum, all[MIDDLE_INDEX_INTERNAL].attrVal, right};
    return insertIntoInternal(relId, attrName, current.pblock, up);
}

int BPlusTree::splitInternal(int blockNum, InternalEntry all[]) {
    // Allocate the right node and read the current node's parent metadata.
    IndInternal left(blockNum);
    IndInternal right;
    int rightNum = right.getBlockNum();
    if (rightNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    HeadInfo lh, rh;
    int ret = left.getHeader(&lh);
    if (ret != SUCCESS) {
        return ret;
    }

    // Keep the middle entry for the parent; split the other entries evenly.
    rh = lh;
    rh.numEntries = MAX_KEYS_INTERNAL / 2;
    rh.lblock = rh.rblock = -1;
    ret = right.setHeader(&rh);
    if (ret != SUCCESS) {
        return ret;
    }

    lh.numEntries = MAX_KEYS_INTERNAL / 2;
    ret = left.setHeader(&lh);
    if (ret != SUCCESS) {
        return ret;
    }

    for (int i = 0; i < lh.numEntries; ++i) {
        ret = left.setEntry(&all[i], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    for (int i = 0; i < rh.numEntries; ++i) {
        ret = right.setEntry(&all[i + MIDDLE_INDEX_INTERNAL + 1], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    // Children moved to the right node must point back to their new parent.
    for (int i = 0; i <= rh.numEntries; ++i) {
        int childNum = (i == 0) ? all[MIDDLE_INDEX_INTERNAL + 1].lChild : all[MIDDLE_INDEX_INTERNAL + i].rChild;
        BlockBuffer child(childNum);
        HeadInfo childHead;
        ret = child.getHeader(&childHead);
        if (ret != SUCCESS) {
            return ret;
        }

        childHead.pblock = rightNum;
        ret = child.setHeader(&childHead);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return rightNum;
}


RecId BPlusTree::bPlusSearch(int relId, char attrName[], Attribute attrVal, int op) {
    // Resume from the last matching index entry when a scan is in progress.
    IndexId searchIndex;
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    // Read the indexed attribute's type and B+ tree root block.
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    int block = -1;
    int index = -1;

    // A reset search index means this is a new scan from the tree root.
    if (searchIndex.block == -1 && searchIndex.index == -1) {
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1) {
            return RecId{-1, -1};
        }
    } else {
        // Continue from the next entry after the previous match.
        block = searchIndex.block;
        index = searchIndex.index + 1;
        IndLeaf leaf(block);
        HeadInfo leafHead;
        ret = leaf.getHeader(&leafHead);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // If the previous leaf is exhausted, continue at its right neighbor.
        if (index >= leafHead.numEntries) {
            block = leafHead.rblock;
            index = 0;

            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    // For a fresh search, descend internal nodes to the first relevant leaf.
    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
        IndInternal internalBlk(block);
        HeadInfo intHead;
        ret = internalBlk.getHeader(&intHead);
        if (ret != SUCCESS || intHead.numEntries == 0) {
            return RecId{-1, -1};
        }

        InternalEntry intEntry;

        // NE must scan the full leaf chain; LT and LE start at the left edge.
        if (op == NE || op == LT || op == LE) {
            ret = internalBlk.getEntry(&intEntry, 0);
            if (ret != SUCCESS) {
                return RecId{-1, -1};
            }
            block = intEntry.lChild;
        } else {
            int i = 0;
            bool entryFound = false;
            while (i < intHead.numEntries) {
                ret = internalBlk.getEntry(&intEntry, i);
                if (ret != SUCCESS) {
                    return RecId{-1, -1};
                }

                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                if ((op == EQ && cmpVal >= 0) ||
                    (op == GE && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)) {
                    entryFound = true;
                    break;
                }

                ++i;
            }

            if (entryFound) {
                // Search the left child first so earlier matching values are found.
                block = intEntry.lChild;
            } else {
                // All separators are smaller, so continue through the rightmost child.
                ret = internalBlk.getEntry(&intEntry, intHead.numEntries - 1);
                if (ret != SUCCESS) {
                    return RecId{-1, -1};
                }
                block = intEntry.rChild;
            }
        }
    }

    // Scan the leaf entries, moving right across leaves when the operator needs it.
    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        ret = leafBlk.getHeader(&leafHead);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        Index leafEntry;
        while (index < leafHead.numEntries) {
            ret = leafBlk.getEntry(&leafEntry, index);
            if (ret != SUCCESS) {
                return RecId{-1, -1};
            }

            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType); 

            if ((op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == NE && cmpVal != 0)) {
                // Save the match so the next call resumes after this entry.
                searchIndex = IndexId{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                // Later leaf entries are even larger, so these operators cannot match.
                return RecId{-1, -1};
            }

            ++index;
        }

        // NE has no ordering shortcut, so it must inspect each leaf in the chain.
        if (op != NE) {
            break;
        }

        block = leafHead.rblock;
        index = 0;
    }

    // No leaf entry satisfied the condition.
    return RecId{-1, -1};
}
