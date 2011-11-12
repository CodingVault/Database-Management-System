#include <fstream>
#include <iostream>
#include <cassert>
#include <vector>

#include "rm.h"
//#include "rm.cc"

#define TABLE_NAME "test"

using namespace std;

const int success = 0;

//void testUpdateDirectory()
//{
//	  Slot slots[9];
//	  Slot slot1;
//	  slot1.flag = 0;
//	  slot1.offset = 0;
//	  slot1.length = 33;
//	  Slot slot2;
//	  slot2.flag = 1;
//	  slot2.offset = 33;
//	  slot2.length = 52;
//	  Slot slot3;
//	  slot3.flag = 0;
//	  slot3.offset = 85;
//	  slot3.length = 69;
//	  Slot slot4;
//	  slot4.flag = 1;
//	  slot4.offset = 154;
//	  slot4.length = 31;
//	  Slot slot5;
//	  slot5.flag = 0;
//	  slot5.offset = 185;
//	  slot5.length = 24;
//	  slots[0] = slot1;
//	  slots[1] = slot2;
//	  slots[2] = slot3;
//	  slots[3] = slot4;
//	  slots[4] = slot5;
//
//	  unsigned count = 4;
//	  updateDirectory(slots, count, 2, 53);
//	  for (unsigned short index = 0; index < count + 1; index++)
//	  {
//		  Slot slot = slots[index];
//		  cout << "Slot: " << slot.flag << ":" << slot.offset << ":" << slot.length << endl;
//	  }
//}

void rmBasicTest()
{
	RM *rm = RM::Instance();
	RC rc = -1;

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
	rc = rm->createTable(TABLE_NAME, attrs);
	assert(rc == success);
	cout << "######## SUCCESS: create table [" << TABLE_NAME << "]." << endl << endl << endl;

	// get attributes
	vector<Attribute> attrs1;
	rc = rm->getAttributes(TABLE_NAME, attrs1);
	for (unsigned index = 0; index < attrs1.size(); index++)
	{
		Attribute attr = attrs1[index];
		cout << "Attribute: " << attr.name << ":" << AttrType(attr.type) << ":" << attr.length << endl;
	}
	assert(rc == success);
	cout << "######## SUCCESS: read attributes from table [" << TABLE_NAME << "]." << endl << endl << endl;

	// insert tuple
	// prepare "id"
	int id = 10;
	// prepare "name"
	char name[] = "record_name";
	int len = strlen(name);
	// prepare tuple
	unsigned short size = 4 + 4 + len;
	char inTuple[size];
	memcpy(inTuple, &id, 4);
	memcpy(inTuple + 4, &len, 4);
	memcpy(inTuple + 8, name, len);
	// insertion
	RID rid;
	rc = rm->insertTuple(TABLE_NAME, inTuple, rid);
	assert(rc == success);
	cout << "######## SUCCESS: insert tuple into table [" << TABLE_NAME << "]." << endl << endl << endl;

	// read tuple
	char outTuple[size];
	rc = rm->readTuple(TABLE_NAME, rid, outTuple);
	// assertion
	assert(rc == success);
	assert(memcmp(&id, outTuple, 4) == success);
	assert(memcmp(&len, outTuple + 4, 4) == success);
	assert(memcmp(name, outTuple + 8, len) == success);
	cout << "######## SUCCESS: read tuple from table [" << TABLE_NAME << "]." << endl << endl << endl;

	// read tuple with invalid RID
	RID ridx;
	ridx.pageNum = 100;
	ridx.slotNum = 200;
	rc = rm->readTuple(TABLE_NAME, ridx, outTuple);
	assert(rc != success);
	cout << "######## SUCCESS: cannot read with invalid RID." << endl << endl << endl;

	// read attribute
	char value[4 + len];
	rc = rm->readAttribute(TABLE_NAME, rid, "name", value);
	assert(rc == success);
	assert(memcmp(&len, value, 4) == success);
	assert(memcmp(name, (char *)value + 4, len) == success);
	cout << "######## SUCCESS: read attribute from table [" << TABLE_NAME << "] at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl << endl << endl;

	// insert another tuple
	// prepare "id"
	int id2 = 20;
	// prepare "name"
	char anotherName[] = "another_record_name";
	int len2 = strlen(anotherName);
	// prepare tuple
	unsigned short size2 = 4 + 4 + len2;
	char inTuple2[size2];
	memcpy(inTuple2, &id2, 4);
	memcpy(inTuple2 + 4, &len2, 4);
	memcpy(inTuple2 + 8, anotherName, len2);
	// insertion
	RID rid2;
	rc = rm->insertTuple(TABLE_NAME, inTuple2, rid2);
	assert(rc == success);
	cout << "######## SUCCESS: insert tuple into table [" << TABLE_NAME << "]." << endl;

	// update tuples
	rc = rm->updateTuple(TABLE_NAME, inTuple, rid2);
	assert(rc == success);
	cout << "######## SUCCESS: update tuple at [" << rid2.pageNum << ":" << rid2.slotNum << "]." << endl;
	rc = rm->updateTuple(TABLE_NAME, inTuple2, rid);
	assert(rc == success);
	cout << "######## SUCCESS: update tuple at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	// read tuple
	rm->readTuple(TABLE_NAME, rid2, outTuple);
	// assertion
	assert(memcmp(&id, outTuple, 4) == success);
	assert(memcmp(&len, outTuple + 4, 4) == success);
	assert(memcmp(name, outTuple + 8, len) == success);
	cout << "######## SUCCESS: read tuple from table [" << TABLE_NAME << "] at [" << rid2.pageNum << ":" << rid2.slotNum << "]." << endl << endl << endl;

	cout << endl << "************** SCAN **************" << endl;
	RM_ScanIterator rmsi;
	string attr = "id";
	vector<string> attributes;
	attributes.push_back(attr);
	char chValue[4];
	int iValue = 20;
	memcpy(chValue, &iValue, 4);
	rc = rm->scan(TABLE_NAME, "id", CompOp(4), chValue, attributes, rmsi);
	assert(rc == success);
	void *data_returned = malloc(100);
	while (rmsi.getNextTuple(rid, data_returned) != RM_EOF)
	{
		cout << "ID: " << *(int *)data_returned << endl;
	}
	free(data_returned);
	cout << "######## SUCCESS: scan table and project all ID fields." << endl << endl << endl;

	// delete tuple
	rc = rm->deleteTuple(TABLE_NAME, rid);
	assert(rc == success);
	cout << "######## SUCCESS: delete tuple from table [" << TABLE_NAME << "] at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	// reading tuple should be invalid
	rc = rm->readTuple(TABLE_NAME, rid, outTuple);
	assert(rc != success);
	// try to delete data already removed
	rc = rm->deleteTuple(TABLE_NAME, rid);
	assert(rc != success);
	cout << "######## SUCCESS: cannot read or delete data already deleted." << endl << endl << endl;

	// delete all the tuples
	rc = rm->deleteTuples(TABLE_NAME);
	assert(rc == success);
	cout << "######## SUCCESS: delete all tuples from table [" << TABLE_NAME << "]." << endl;
	// reading tuple should be invalid
	rc = rm->readTuple(TABLE_NAME, rid, outTuple);
	assert(rc != success);
	cout << "######## SUCCESS: cannot read data already deleted." << endl << endl << endl;

	// re-organize page
	rc = rm->reorganizePage(TABLE_NAME, 0);
	assert(rc == success);
	cout << "######## SUCCESS: re-organized table [" << TABLE_NAME << "]." << endl;
	rc = rm->readTuple(TABLE_NAME, rid, outTuple);
	assert(rc != success);
	cout << "######## SUCCESS: cannot read data already deleted." << endl << endl << endl;
}

void rmTableRebuildTest()
{
	RM *rm = RM::Instance();

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
	rm->createTable(TABLE_NAME, attrs);
	cout << "######## SUCCESS: create table [" << TABLE_NAME << "]." << endl << endl << endl;

	// get attributes
	vector<Attribute> attrs1;
	rm->getAttributes(TABLE_NAME, attrs1);
	for (unsigned index = 0; index < attrs1.size(); index++)
	{
		Attribute attr = attrs1[index];
		cout << "Attribute: " << attr.name << ":" << AttrType(attr.type) << ":" << attr.length << endl;
	}
	cout << "######## SUCCESS: read attributes from table [" << TABLE_NAME << "]." << endl << endl << endl;

	// delete table
	RC rc = rm->deleteTable(TABLE_NAME);
	assert(rc == success);
	cout << "######## SUCCESS: deleted table [" << TABLE_NAME << "]." << endl << endl << endl;

	// create table "test"
	attrs.resize(3);
	attr1.name = "extra";
	attr1.type = AttrType(2);
	attr1.length = 20;
	attr2.name = "id";
	attr2.type = AttrType(0);
	attr2.length = 4;
	Attribute attr3;
	attr3.name = "name";
	attr3.type = AttrType(2);
	attr3.length = 20;
	attrs[0] = attr1;
	attrs[1] = attr2;
	attrs[2] = attr3;
	rc = rm->createTable(TABLE_NAME, attrs);
	assert(rc == success);
	cout << "######## SUCCESS: create table [" << TABLE_NAME << "] again with different attributes." << endl;

	// get attributes
	vector<Attribute> attrs3;
	rc = rm->getAttributes(TABLE_NAME, attrs3);
	assert(rc == success);
	for (unsigned index = 0; index < attrs3.size(); index++)
	{
		Attribute attr = attrs3[index];
		cout << "Attribute: " << attr.name << "!" << AttrType(attr.type) << "!" << attr.length << endl;
	}
	cout << "######## SUCCESS: read attributes from table [" << TABLE_NAME << "]." << endl << endl << endl;
}

void rmAddAttributeTest()
{
	RM *rm = RM::Instance();

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
	RC rc = rm->createTable(TABLE_NAME, attrs);
	cout << "######## SUCCESS: create table [" << TABLE_NAME << "]." << endl << endl << endl;

	// get attributes
	vector<Attribute> attrs1;
	rm->getAttributes(TABLE_NAME, attrs1);
	for (unsigned index = 0; index < attrs1.size(); index++)
	{
		Attribute attr = attrs1[index];
		cout << "Attribute: " << attr.name << ":" << AttrType(attr.type) << ":" << attr.length << endl;
	}
	cout << "######## SUCCESS: read attributes from table [" << TABLE_NAME << "]." << endl << endl << endl;

	// insert tuple
	// prepare "id"
	int id = 10;
	// prepare "name"
	char name[] = "record_name";
	int len = strlen(name);
	// prepare tuple
	unsigned short size = 4 + 4 + len;
	char inTuple[size];
	memcpy(inTuple, &id, 4);
	memcpy(inTuple + 4, &len, 4);
	memcpy(inTuple + 8, name, len);
	// insertion
	RID rid;
	rm->insertTuple(TABLE_NAME, inTuple, rid);
	cout << "######## SUCCESS: insert tuple into table [" << TABLE_NAME << "]." << endl << endl << endl;

	// read tuple
	char outTuple[size];
	rm->readTuple(TABLE_NAME, rid, outTuple);
	// assertion
	assert(memcmp(&id, outTuple, 4) == success);
	assert(memcmp(&len, outTuple + 4, 4) == success);
	assert(memcmp(name, outTuple + 8, len) == success);
	cout << "######## SUCCESS: read tuple from table [" << TABLE_NAME << "]." << endl << endl << endl;

	// add new attributes
	Attribute newAttr1;
	newAttr1.name = "extraFloat";
	newAttr1.type = AttrType(1);
	newAttr1.length = 4;
	rc = rm->addAttribute(TABLE_NAME, newAttr1);
	assert(rc == success);
	cout << "######## SUCCESS: add new attribute [" << newAttr1.name << "] to table [" << TABLE_NAME << "]." << endl;
	Attribute newAttr2;
	newAttr2.name = "extraVarChar";
	newAttr2.type = AttrType(2);
	newAttr2.length = 30;
	rc = rm->addAttribute(TABLE_NAME, newAttr2);
	assert(rc == success);
	cout << "######## SUCCESS: add new attribute [" << newAttr2.name << "] to table [" << TABLE_NAME << "]." << endl;
	newAttr2.name = "extraVarChar";
	newAttr2.type = AttrType(2);
	newAttr2.length = 30;
	rc = rm->addAttribute(TABLE_NAME, newAttr2);
	assert(rc != success);
	cout << "######## SUCCESS: cannot add an attribute already exists to table [" << TABLE_NAME << "]." << endl;
	// get attributes
	attrs1.clear();
	rm->getAttributes(TABLE_NAME, attrs1);
	for (unsigned index = 0; index < attrs1.size(); index++)
	{
		Attribute attr = attrs1[index];
		cout << "Attribute: " << attr.name << ":" << AttrType(attr.type) << ":" << attr.length << endl;
	}
	cout << "######## SUCCESS: read attributes from table [" << TABLE_NAME << "]." << endl << endl << endl;

	// read tuple
	char newOutTuple[size + 10];
	rc = rm->readTuple(TABLE_NAME, rid, newOutTuple);
	// assertion
	assert(rc == success);
	assert(memcmp(&id, newOutTuple, 4) == success);
	assert(memcmp(&len, newOutTuple + 4, 4) == success);
	assert(memcmp(name, newOutTuple + 8, len) == success);
	float fDefault = 0.0F;
	unsigned lenDefault = 0;
	string empty = "";
	assert(memcmp(&fDefault, newOutTuple + 8 + len, 4) == success);
	assert(memcmp(&lenDefault, newOutTuple + 12 + len, 4) == success);
	assert(memcmp(empty.c_str(), newOutTuple + 16 + len, 1) == success);
	cout << "######## SUCCESS: read tuple from table [" << TABLE_NAME << "] after adding attributes." << endl << endl << endl;

	// read attribute
	char value[10];
	string attr = "extraFloat";
	rc = rm->readAttribute(TABLE_NAME, rid, attr, value);
	assert(rc == success);
	assert(memcmp(&fDefault, value, 4) == success);
	cout << "Value read is: " << *(float *)value << endl;
	cout << "######## SUCCESS: read attribute [" << attr << "] from table [" << TABLE_NAME << "] at ";
	cout << "[" << rid.pageNum << ":" << rid.slotNum << "] after adding attributes." << endl;
	attr = "extraVarChar";
	rc = rm->readAttribute(TABLE_NAME, rid, attr, value);
	assert(rc == success);
	assert(memcmp(&lenDefault, value, 4) == success);
	assert(memcmp(empty.c_str(), (char *)value + 4, 1) == success);
	cout << "Value read is: " << *(char *)value << endl;
	cout << "######## SUCCESS: read attribute [" << attr << "] from table [" << TABLE_NAME << "] at ";
	cout << "[" << rid.pageNum << ":" << rid.slotNum << "] after adding attributes." << endl << endl << endl;
}

void rmReorganizeTableTest()
{
	rmBasicTest();

	RM *rm = RM::Instance();

	// insert tuple
	// prepare "id"
	int id = 10;
	// prepare "name"
	char name[] = "record_name";
	int len = strlen(name);
	// prepare tuple
	unsigned short size = 4 + 4 + len;
	char inTuple[size];
	memcpy(inTuple, &id, 4);
	memcpy(inTuple + 4, &len, 4);
	memcpy(inTuple + 8, name, len);
	// insertion
	RID rid;
	rm->insertTuple(TABLE_NAME, inTuple, rid);
	cout << "######## SUCCESS: insert tuple into table [" << TABLE_NAME << "]." << endl << endl << endl;

	// re-organize table
	RC rc = rm->reorganizeTable(TABLE_NAME);
	assert(rc == success);
	cout << "######## SUCCESS: re-organize table [" << TABLE_NAME << "]." << endl << endl << endl;

	// read tuple
	char outTuple[size];
	rc = rm->readTuple(TABLE_NAME, rid, outTuple);
	// assertion
	assert(rc != success);
	rid.slotNum = 0;
	rc = rm->readTuple(TABLE_NAME, rid, outTuple);
	assert(memcmp(&id, outTuple, 4) == success);
	assert(memcmp(&len, outTuple + 4, 4) == success);
	assert(memcmp(name, outTuple + 8, len) == success);
	cout << "######## SUCCESS: read tuple from table [" << TABLE_NAME << "]." << endl << endl << endl;
}

int main()
{
  cout << "test..." << endl;

//  rmBasicTest();
//  rmTableRebuildTest();
//  rmAddAttributeTest();
//  rmReorganizeTableTest();

//  testUpdateDirectory();

  cout << "OK" << endl;
}
