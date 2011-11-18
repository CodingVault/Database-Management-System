
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
	RM *rm = RM::Instance();
	RC rc = -1;
	string tableName = "test";

	vector<Attribute> attrs;
	// get attributes
	rm->getAttributes("catalog", attrs);
	for (unsigned index = 0; index < attrs.size(); index++)
	{
		Attribute attr = attrs[index];
		cout << "Attribute: " << attr.name << ":" << AttrType(attr.type) << ":" << attr.length << endl;
	}
	cout << "######## SUCCESS: read attributes from catalog." << endl << endl << endl;

	// create table "test"
	attrs.resize(2);
	Attribute attr1;
	attr1.name = "id";
	attr1.type = AttrType(0);
	attr1.length = 4;
	Attribute attr2;
	attr2.name = "name";
	attr2.type = AttrType(2);
	attr2.length = 20;
	attrs[0] = attr1;
	attrs[1] = attr2;
	rc = rm->createTable(tableName, attrs);
	assert(rc == SUCCESS);
	cout << "######## SUCCESS: create table [" << tableName << "]." << endl << endl << endl;

	IX_Manager *ix_manager = IX_Manager::Instance();
	IX_IndexHandle ix_handle;
	rc = ix_manager->CreateIndex(tableName, attr1.name);
	assert(rc == SUCCESS);
	cout << "==== Create index done!" << endl << endl;
	rc = ix_manager->OpenIndex(tableName, attr1.name, ix_handle);
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
		rc = insertEntry(ix_handle, key, (pageNum + index * 8) % 20, (slotNum * index + 10) % 50);
	}
	assert(rc == SUCCESS);
	cout << "==== Insert entry done!" << endl << endl;
	rc = ix_manager->CloseIndex(ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Close index done!" << endl << endl;
	rc = ix_manager->DestroyIndex(tableName, attr1.name);
	assert(rc == SUCCESS);
	cout << "==== Destroy index done!" << endl << endl;
}

int main() 
{
  cout << "test..." << endl;

  ixTest();

  cout << "OK" << endl;
}
