
#include <fstream>
#include <iostream>
#include <cassert>

#include "ix.h"

using namespace std;

void ixTest()
{
	unsigned key = 1;
	RID rid;
	rid.pageNum = 2;
	rid.slotNum = 3;

	IX_Manager *ix_manager = IX_Manager::Instance();
	IX_IndexHandle ix_handle;
	RC rc = ix_manager->CreateIndex("Emp", "eno");
	assert(rc == SUCCESS);
	cout << "==== Create index done!" << endl << endl;
	rc = ix_manager->OpenIndex("Emp", "eno", ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Open index done!" << endl << endl;
	rc = ix_handle.InsertEntry(&key, rid);
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
