
#include <iostream>
#include <cassert>

#include "ix.h"

using namespace std;

RC insertEntry(IX_IndexHandle &handle, unsigned key, const unsigned pageNum, const unsigned slotNum)
{
	RID rid;
	rid.pageNum = pageNum;
	rid.slotNum = slotNum;
	cout << "==========>>> TEST: inserting (" << key << ") -> [" << pageNum << ":" << slotNum << "]." << endl;
	return handle.InsertEntry(&key, rid);
}

void ixTest()
{
	IX_Manager *ix_manager = IX_Manager::Instance();
	IX_IndexHandle ix_handle;
	RC rc = ix_manager->CreateIndex("Emp", "eno");
	assert(rc == SUCCESS);
	cout << "==== Create index done!" << endl << endl;
	rc = ix_manager->OpenIndex("Emp", "eno", ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Open index done!" << endl << endl;
	unsigned pageNum = 0;
	unsigned slotNum = 0;
	rc = insertEntry(ix_handle, 1, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 2, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 7, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 5, pageNum++, slotNum);
	rc = insertEntry(ix_handle, 6, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 3, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 4, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 8, pageNum, slotNum++);
	rc = insertEntry(ix_handle, 9, pageNum, slotNum++);
	for (unsigned index = 0; index < 1000; index++)
	{
		unsigned key = (index * 1000000 - 123245) % 341231;
		rc = insertEntry(ix_handle, key, (pageNum + index * 8) % 10, (slotNum * index + 10) % 20);
	}
	assert(rc == SUCCESS);
	cout << "==== Insert entry done!" << endl << endl;
	rc = ix_manager->CloseIndex(ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Close index done!" << endl << endl;
	rc = ix_manager->DestroyIndex("Emp", "eno");
	assert(rc == SUCCESS);
	cout << "==== Destroy index done!" << endl << endl;
}

int main() 
{
  cout << "test..." << endl;

  ixTest();

  cout << "OK" << endl;
}
