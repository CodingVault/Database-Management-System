
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
	ix_manager->CreateIndex("Emp", "eno");
	ix_manager->OpenIndex("Emp", "eno", ix_handle);
	ix_handle.InsertEntry(&key, rid);
	ix_manager->CloseIndex(ix_handle);
	ix_manager->DestroyIndex("Emp", "eno");
}

int main() 
{
  cout << "test..." << endl;

  ixTest();

  cout << "OK" << endl;
}
