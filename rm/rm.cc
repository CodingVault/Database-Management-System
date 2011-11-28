#include "rm.h"
#include <iostream>
#include <vector>

#define DEBUG false

RM* RM::_rm = 0;
PF_Manager* RM::_pfManager = 0;
vector<AttributeCache> RM::_attrsCache;	// TODO: re-factor size
vector<void *> RM::_projectedTuples;

RM* RM::Instance()
{
    if(!_rm)
        _rm = new RM();
    
    return _rm;
}

RM::RM()
{
	this->initCatalog();
	_pfManager = PF_Manager::Instance();
}

RM::~RM()
{
	_pfManager = 0;
}

void prepareAttributes(void*);

RC RM::initCatalog()
{
	RC rc = _pfManager->CreateFile(CATALOG_FILE_NAME);
	if (rc == -1)
	{
		cout << "RM::initCatalog - Using the existing catalog file." << endl;
		return 0;
	}
	else if (rc == -2)
	{
		cerr << "RM::initCatalog - Cannot initialize catalog file for database." << endl;
		return -2;
	}

	char *page = (char *) malloc(PF_PAGE_SIZE);
	prepareAttributes(page);

	PF_FileHandle handle;
	_pfManager->OpenFile(CATALOG_FILE_NAME, handle);
	handle.WritePage(0, page);
	_pfManager->CloseFile(handle);

	free(page);
	return 0;
}

void report(const Slot *slots, const unsigned count)
{
	cout << "==============================================" << endl;
	cout << "Page usage status: " << count << " slots in total." << endl;
	cout << "	Total space - " << PF_PAGE_SIZE << " bytes." << endl;
	cout << "	Data - " << slots[count].offset << " bytes (including slots with free space)." << endl;
	cout << "	Slot directory - " << EACH_SLOT_INFO_LEN * count + PAGE_CONTROL_INFO_LEN << " bytes." << endl;
	cout << "	Free space - " << slots[count].length << " bytes." << endl;
	cout << "==============================================" << endl;
}

void init(Slot *slots)
{
	Slot empty;
	empty.flag = 0;
	empty.offset = 0;
	empty.length = PF_PAGE_SIZE - PAGE_CONTROL_INFO_LEN - PAGE_PRESCRIBED_FREE_SPACE;
	slots[0] = empty;
}

/*
 * Writes directory info on page.
 */
void writeDirectory(void *page, const Slot *slots, const unsigned count)
{
	unsigned dataCount = 0;
	unsigned offset = PF_PAGE_SIZE - PAGE_CONTROL_INFO_LEN - EACH_SLOT_INFO_LEN * count;
	memcpy((char *)page + PAGE_CONTROL_INFO_POS, &offset, 2);

	for (unsigned index = 0; index < count; index++)
	{
		// add one slot record on page reversely
		Slot slot = slots[index];
		char* slotPos = (char *)page + PF_PAGE_SIZE - PAGE_CONTROL_INFO_LEN - EACH_SLOT_INFO_LEN * (index + 1);
		memcpy(slotPos, &slot.flag, 2);
		memcpy(slotPos + 2, &slot.offset, 2);
		memcpy(slotPos + 4, &slot.length, 2);

		if (slot.flag != 3)
			dataCount++;
	}

	cout << "RM::updateSlotDirectory - Slot directory info updated. ";
	cout << "There are now " << count << " slots in total [" << dataCount << " valid one(s)]." << endl;
	if (DEBUG)
		report(slots, count);
}

/*
 * Reads directory info from page, appending one empty slot at the beginning of slots array.
 * The empty slot stores the last offset of empty space, and the bytes left prior to directory
 * >> minus prescribed buff space for update << (0 if minus).
 *
 * @return slot count, excluding the last empty slot.
 */
unsigned readDirectory(const void *page, Slot *slots)
{
	// read the position of last slot in directory
	unsigned short last = 0;
	memcpy(&last, (char *)page + PAGE_CONTROL_INFO_POS, 2);

	Slot lastValidSlot;
	lastValidSlot.flag = 0;
	lastValidSlot.offset = 0;
	lastValidSlot.length = 0;
	unsigned short count = (PF_PAGE_SIZE - last - PAGE_CONTROL_INFO_LEN) / 6;
	if (DEBUG)
		cout << "RM::readSlotDirectory - Reading " << count << " slot(s) directory info from page." << endl;
	if (last > 0)
	{
		for (unsigned index = 0; index < count; index++)
		{
			char *slotPos = (char *)page + PAGE_CONTROL_INFO_POS - EACH_SLOT_INFO_LEN * (index + 1);
			Slot slot;
			memcpy(&slot.flag, slotPos, 2);
			memcpy(&slot.offset, slotPos + 2, 2);
			memcpy(&slot.length, slotPos + 4, 2);
			slots[index] = slot;
			if (DEBUG)
				cout << "RM::readSlotDirectory - Data read: " << slot.flag << "|" << slot.offset << "|" << slot.length << endl;

			if (slot.flag != 3 && slot.offset >= lastValidSlot.offset)
				lastValidSlot = slot;
		}
	}

	// append empty slot
	Slot empty;
	empty.flag = 0;
	empty.offset = lastValidSlot.offset + lastValidSlot.length;
	short freeSpace = last - empty.offset - PAGE_PRESCRIBED_FREE_SPACE;
	empty.length = freeSpace >= 0 ? freeSpace : 0;
	slots[count] = empty;
	if (DEBUG)
		cout << "RM::readSlotDirectory - Empty slot: " << empty.flag << "|" << empty.offset << "|" << empty.length << endl;

	if (DEBUG)
		report(slots, count);
	return count;
}

/*
 * Updates directory in memory (slotNum starts from 0, count starts from 1).
 *
 * !!! COMPLEX LOGIC !!!
 * If the length of new slot is less than the original one, leave new length in the original slot,
		and create new slot pointing to the free space left (insert the slot info at the end of directory);
 * if it is greater than the original one, update it and move all slots afterward;
 * if they are equal, just update flag, cheers!
 *
 * @para slots is in-memory directory.
 * @para count is current count of slots in directory, and will be updated if necessary.
 * @para slotNum is the place to be updated.
 * @para size is the new length of the corresponding slot.
 */
void updateDirectory(Slot *slots, unsigned &count, const unsigned slotNum, const unsigned size)
{
	Slot update = slots[slotNum];
	int discrepancy = size - update.length; // how much the length of new data is greater than original one
	update.flag = 1;
	update.length = size;

	if (discrepancy < 0)		// TODO: HIGH!!! if slotNum = count - 1, merge two tailing free space
	{
		Slot empty = slots[count];	// must be retrieved first in case new slot is just the last empty slot
		Slot newSlot;	// points to free space left after part of the empty slot is occupied
		newSlot.flag = 0;
		newSlot.offset = update.offset + size;
		newSlot.length = -(discrepancy);

		// if slotNum == count, the new slot info will be transferred to empty slot
		slots[count] = newSlot;
		cout << "HELPER::updateDirectory - Adding newSlot " << count << " [size: " << newSlot.length << "] starting from offset [" << newSlot.offset << "]." << endl;
		slots[slotNum] = update;	// must be after newSlot info is written in case slotNum == count
		if (slotNum == count)
			empty = newSlot;
		empty.length -= EACH_SLOT_INFO_LEN;
		slots[++count] = empty;

		if (DEBUG)
			report(slots, count);
		return;
	}
	else if (discrepancy > 0)
	{
		for (unsigned index = slotNum + 1; index <= count; index++)
			slots[index].offset += discrepancy;
		slots[count].length -= discrepancy;
	}

	slots[slotNum] = update;
}

/*
 * Tries to find space of size in current directory of count slots.
 *
 * @returns available slot position in directory, -1 if not found.
 */
short findSpace(const Slot *slots, const unsigned short count, const unsigned size)
{
	for (unsigned index = 0; index <= count; index++)
	{
		unsigned availSpace = 0;
		if (slots[index].length >= 0)
			availSpace = (unsigned) slots[index].length;
		else
			continue;

		if (slots[index].flag == 0 && availSpace >= size)
			return index;
	}

	return -1;
}

void updatePageData(void *page, const void *data, const unsigned short offset, const unsigned dataSize)
{
	memcpy((char *)page + offset, data, dataSize);
}

/**
 *
 */
void getAttributeValue(const void *tuple, const vector<Attribute> &attrs, const char *attrName, void *data)
{
	// load the count of fields
	unsigned fieldsCount = 0;
	memcpy(&fieldsCount, tuple, 4);
	cout << "Helper::getAttributeValue - Looking up " << fieldsCount << " fields in current record for attribute [" << attrName << "]." << endl;

	// try to read attribute value from current record
	unsigned short offset = 4;
	for (unsigned index = 0; index < fieldsCount; index++)
	{
		cout << "Helper::getAttributeValue - Reading attribute: " << attrs[index].name << "|" << attrs[index].type << endl;
		unsigned len = 4;
	    if (attrs[index].type == AttrType(2))
	    {
	    	int fieldLen = 0;
	        memcpy(&fieldLen, (char *)tuple + offset, 4);
	        len += fieldLen;
	    }
        cout << "Helper::getAttributeValue - Data start from " << offset << " with length of " << len << "." << endl;

	    if (strcmp(attrs[index].name.c_str(), attrName) == 0)
	    {
	    	cout << "Helper::getAttributeValue - Found value for attribute [" << attrName << "]." << endl;
	    	memcpy(data, (char *)tuple + offset, len);
	        return;
	    }

	    offset += len;
	}

	// do not find given attribute in current record -- the attribute was added after the record was inserted
	// return default value
	while (fieldsCount < attrs.size())
	{
	    if (strcmp(attrs[fieldsCount].name.c_str(), attrName) == 0)
	    {
	    	cout << "Helper::getAttributeValue - Attribute is added after record was stored to database; return default value." << endl;
	    	int iDefault = 0;
	    	float fDefault = 0.0F;
	    	int len = 0;
	    	switch (attrs[fieldsCount].type)
	    	{
	    	case AttrType(0):
	    			memcpy(data, &iDefault, 4);
	    			return;
	    	case AttrType(1):
	    			memcpy(data, &fDefault, 4);
	    			return;
	    	case AttrType(2):
	    			memcpy(data, &len, 4);
	    			return;
	    	}
	    }
		fieldsCount++;
	}
}

/*
 * Concatenates source to destination and returns the size of the field.
 */
unsigned fieldcat(char *destination, const char* source)
{
	unsigned len = strlen(source);
	memcpy(destination, &len, 4);
	memcpy(destination + 4, source, len);

	return (4 + len);
}

unsigned prepareAttribute(char *record, const char *tableName, const Attribute &attr, const unsigned position)
{
	unsigned size = 0;

	// count of fields
	unsigned fieldcount = 5;		// TODO: put it as a constant
	memcpy(record, &fieldcount, 4);
	size += 4;

	// table name
	size += fieldcat(record + size, tableName);

	// attribute name
	size += fieldcat(record + size, attr.name.c_str());

	// attribute type
	memcpy(record + size, &attr.type, 4);
	size += 4;

	// attribute length
	memcpy(record + size, &attr.length, 4);
	size += 4;

	// position
	memcpy(record + size, &position, 4);
	size += 4;

	return size;
}

void prepareAttributes(void *page)
{
	cout << "==== Entering RM::prepareAttributes." << endl;

	string columnNames[] = {
			"TABLE_NAME",
			"COLUMN_NAME",
			"TYPE",
			"LENGTH",
			"POSITION",
	};
	AttrType types[] = {
			AttrType(2),
			AttrType(2),
			AttrType(0),
			AttrType(0),
			AttrType(0),
	};
	unsigned lens[] = {20, 20, 4, 4, 4, 2, 20};

	Slot slots[SLOTS_CAPACITY];
	init(slots);

	unsigned offset = 0;
	unsigned count = 0;
	for (unsigned index = 0; index < 5; index++)
	{
		Attribute attr;
		attr.name = columnNames[index];
		attr.type = types[index];
		attr.length = lens[index];

		char record[100];
		unsigned size = prepareAttribute(record, CATALOG_TABLE_NAME, attr, index + 1);
		memcpy((char *)page + offset, record, size);

		char data[size];
		memcpy(data, (char *)page + offset, size);
		offset += size;

		updateDirectory(slots, count, index, size);
	}
	writeDirectory(page, slots, 5);

	cout << "==== Leaving RM::prepareAttributes." << endl;
}

// TODO: need to update for attribute modification with regards to position field in catalog
unsigned recordSizeof(const void *record, const vector<Attribute> attrs)
{
	unsigned size = 0;	// init with offset skipping attribute count
	for (unsigned index = 0; index < attrs.size(); index++)
	{
		Attribute attr = attrs[index];
		switch (attr.type)
		{
		case 0:
			size += 4;
			break;
		case 1:
			size += 4;
			break;
		case 2:
			unsigned len = 0;
			memcpy(&len, (char *)record + size, 4);
			size += 4 + len;
			break;
		}
	}
	return size;
}

void appendFieldCount(void *record, const void *data, unsigned &size, const unsigned fieldsCount)
{
	cout << "HELPER::appendFieldCount - Appending the count of fields [" << fieldsCount << "] to tuple." << endl;
	memcpy(record, &fieldsCount, 4);
	memcpy((char *)record + 4, data, size);
	size += 4;
}

void appendFields(void *data, unsigned &size, const Attribute attr)
{
	// default values
	int iDefault = 0;
	float fDefault = 0.0F;
	int len = 0;

	switch (attr.type)
	{
		case AttrType(0):
			cout << "appendFields - Appending default int value (" << iDefault << ")." << endl;
			memcpy((char *)data + size, &iDefault, 4);
			break;
		case AttrType(1):
			cout << "appendFields - Appending default float value (" << fDefault << ")." << endl;
			memcpy((char *)data + size, &fDefault, 4);
			break;
		case AttrType(2):
			cout << "appendFields - Appending default varchar value ("")." << endl;
			memcpy((char *)data + size, &len, 4);
			break;
	}
	size += 4;
}

void validateTuple(void *data, unsigned &size, const vector<Attribute> attrs)
{
	unsigned fieldsCount = 0;
	memcpy(&fieldsCount, data, 4);
	cout << "HELPER::validateTuple - Validating the record (length: " << size << ") containing " << fieldsCount << " fields for now." << endl;

	if (fieldsCount < attrs.size())
	{
		cout << "HELPER::validateTuple - Current count of attributes is " << attrs.size() << "; need to append default value(s)." << endl;
		while (fieldsCount < attrs.size())
		{
			appendFields(data, size, attrs[fieldsCount]);
			fieldsCount++;
		}
	}

	memcpy(data, &fieldsCount, 4);
	cout << "HELPER::validateTuple - Done validation; " << fieldsCount << " fields in total; record length is " << size << "." << endl;
}

RC RM::createTable(const string tableName, const vector<Attribute> &attrs)
{
	// create table file
	RC rc = _pfManager->CreateFile(APPEND_TABLE_SUFFIX(tableName));
	if (rc == -1)
	{
		cout << "A table with name \"" << tableName << "\" has already existed." << endl;
		return -1;
	}
	else if (rc == -2)
	{
		cerr << "Failed to create table \"" << tableName << "\"." << endl;
		return -2;
	}

	// update catalog
	PF_FileHandle handle;
	if (_pfManager->OpenFile(CATALOG_FILE_NAME, handle) != 0)
	{
		cerr << "Failed to open data file \"" << tableName << "\"." << endl;
		return -1;
	}

	this->initTableAttributes(handle, tableName.c_str(), attrs);

	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "Failed to close data file \"" << tableName << "\"." << endl;
		return -1;
	}

	return 0;
}

RC RM::deleteTable(const string tableName)
{
	if (_pfManager->DestroyFile(APPEND_TABLE_SUFFIX(tableName)) != 0)
	{
		cerr << "Failed to delete table \"" << tableName << "\"." << endl;
		return -2;
	}

	PF_FileHandle handle;
	if (_pfManager->OpenFile(CATALOG_FILE_NAME, handle) != 0)
	{
		cerr << "Failed to open catalog file \"" << CATALOG_FILE_NAME << "\"." << endl;
		return -1;
	}

	this->markDeletion(handle, tableName.c_str());

	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "Failed to close catalog file \"" << CATALOG_FILE_NAME << "\"." << endl;
		return -1;
	}

	return 0;
}

RC RM::getAttributes(const string tableName, vector<Attribute> &attrs)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(CATALOG_FILE_NAME, handle) != 0)
	{
		cerr << "Failed to open catalog file \"" << CATALOG_FILE_NAME << "\"." << endl;
		return -1;
	}

	this->loadAttributes(handle, tableName.c_str(), attrs);

	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "Failed to close catalog file \"" << CATALOG_FILE_NAME << "\"." << endl;
		return -1;
	}

	return 0;
}

//  Format of the data passed into the function is the following:
//  1) data is a concatenation of values of the attributes
//  2) For int and real: use 4 bytes to store the value;
//     For varchar: use 4 bytes to store the length of characters, then store the actual characters.
//  !!!The same format is used for updateTuple(), the returned data of readTuple(), and readAttribute()
RC RM::insertTuple(const string tableName, const void *data, RID &rid)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::insertTuple - Failed to open data file \"" << tableName << "\"." << endl;
		return -1;
	}

	cout << "RM::insertTuple - Inserting one tuple into table [" << tableName << "]." << endl;
	vector<Attribute> attrs;
	getAttributes(tableName, attrs);
	unsigned size = recordSizeof(data, attrs);
	cout << "RM::insertTuple - Loaded " << attrs.size() << " attribute(s) from catalog ";
	cout << "for table [" << tableName << "]." << endl;
	cout << "RM::insertTuple - The length of input tuple is " << size << "." << endl;
	void *record = malloc(size + 4);
	appendFieldCount(record, data, size, attrs.size());
	RC rc = this->insert(handle, record, size, rid);
	free(record);

	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::insertTuple - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::insertTuple - Failed to insert tuple into table [" << tableName << "]." << endl;
		return -2;
	}

	cout << "RM::insertTuple - Successfully inserted new data [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	return 0;
}

RC RM::readTuple(const string tableName, const RID &rid, void *data)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::readTuple - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	cout << "RM::readTuple - Reading one tuple from table [" << tableName << "] ";
	cout << "at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	unsigned recordSize;
	RC rc = this->read(handle, rid, data, recordSize);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::readTuple - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::readTuple - Failed to read tuple from table [" << tableName << "]." << endl;
		return -2;
	}

	vector<Attribute> attrs;
	this->getAttributes(tableName, attrs);
	validateTuple(data, recordSize, attrs);

	memcpy(data, (char *)data + 4, recordSize - 4);		// remove field count
	cout << "RM::readTuple - Successfully read tuple from table [" << tableName << "]." << endl;
	return 0;
}

// Assume the rid does not change after update
RC RM::updateTuple(const string tableName, const void *data, const RID &rid)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::updateTuple - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	cout << "RM::updateTuple - Updating one tuple in table [" << tableName << "]." << endl;
	vector<Attribute> attrs;
	getAttributes(tableName, attrs);	// TODO: LOW! what if failed?
	unsigned size = recordSizeof(data, attrs);
	cout << "RM::updateTuple - Reading one tuple from table [" << tableName << "] ";
	cout << "at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	cout << "RM::updateTuple - The length of input tuple is " << size << "." << endl;

	void *record = malloc(size + 4);
	appendFieldCount(record, data, size, attrs.size());
	RC rc = this->update(handle, record, size, rid);
	free(record);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::updateTuple - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::updateTuple - Failed to update tuple in table [" << tableName << "]." << endl;
		return -2;
	}

	cout << "RM::updateTuple - Successfully updated tuple in table [" << tableName << "]." << endl;
	return 0;
}

RC RM::readAttribute(const string tableName, const RID &rid, const string attributeName, void *data)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::readAttribute - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	char tuple[PF_PAGE_SIZE];	// TODO: update it
	unsigned size;
	RC rc = this->read(handle, rid, tuple, size);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::readAttribute - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::readAttribute - Failed to load the tuple [" << attributeName << "] from table [" << tableName << "] ";
		cerr << "at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
		return -2;
	}

	vector<Attribute> attrs;
	getAttributes(tableName, attrs);	// TODO: LOW! what if failed?

	getAttributeValue(tuple, attrs, attributeName.c_str(), data);

	cout << "RM::readAttribute - Loaded the attribute [" << attributeName << "] from table [" << tableName << "] ";
	cout << "at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	return 0;
}

RC RM::deleteTuple(const string tableName, const RID &rid)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::deleteTuple - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	RC rc = this->remove(handle, rid);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::deleteTuple - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::deleteTuple - Failed to delete tuple in table [" << tableName << "]." << endl;
		return -2;
	}

	cout << "RM::deleteTuple - Successfully deleted tuple in table [" << tableName << "]." << endl;
	return 0;
}

RC RM::deleteTuples(const string tableName)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::deleteTuples - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	RC rc = clearTuples(handle);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::deleteTuples - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::deleteTuples - Failed to delete all the tuples in table [" << tableName << "]." << endl;
		return -2;
	}

	cout << "RM::deleteTuples - Successfully deleted all the tuples in table [" << tableName << "]." << endl;
	return 0;
}

RC RM::reorganizePage(const string tableName, const unsigned pageNumber)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::reorganizePage - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	unsigned totalPageNum = handle.GetNumberOfPages();
	if (pageNumber >= totalPageNum)
	{
		cerr << "RM::reorganizePage - Page number " << pageNumber << " does not exist in table [" << tableName << "]." << endl;
		return -3;
	}

	RC rc = this->reorganizePG(handle, pageNumber);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::reorganizePage - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::reorganizePage - Failed to re-organize page " << pageNumber << " in table [" << tableName << "]." << endl;
		return -2;
	}

	cout << "RM::reorganizePage - Successfully re-organized page " << pageNumber << " in table [" << tableName << "]." << endl;
	return 0;
}

// scan returns an iterator to allow the caller to go through the results one by one.
RC RM::scan(const string tableName,
    const string conditionAttribute,
    const CompOp compOp,                  // comparision type such as "<" and "="
    const void *value,                    // used in the comparison
    const vector<string> &attributeNames, // a list of projected attributes
    RM_ScanIterator &rm_ScanIterator)
{
	rm_ScanIterator.setTableName(tableName);
	rm_ScanIterator.setCondAttr(conditionAttribute);
	rm_ScanIterator.setCompOp(compOp);
	if (value)
		rm_ScanIterator.setValue(value);
	rm_ScanIterator.setAttrNames(attributeNames);

	RC rc = rm_ScanIterator.startReading();

	if (rc != 0)
	{
		cerr << "RM::scan - Failed to table [" << tableName << "]." << endl;
		return -1;
	}

	cout << "RM::scan - Successfully scanned table [" << tableName << "]." << endl;
	return 0;
}

RC RM::addAttribute(const string tableName, const Attribute attr)	// TODO: update cache
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(CATALOG_FILE_NAME, handle) != 0)
	{
		cerr << "Failed to open catalog file [" << CATALOG_FILE_NAME << "]." << endl;
		return -1;
	}

	RC rc = this->appendAttribute(handle, tableName.c_str(), attr);

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "Failed to close catalog file [" << CATALOG_FILE_NAME << "]." << endl;
		return -1;
	}

	if (rc != 0)
	{
		cerr << "RM::addAttribute - Failed to add new attribute to table [" << tableName << "]." << endl;
		return -2;
	}

	return 0;
}

RC RM::reorganizeTable(const string tableName)
{
	PF_FileHandle handle;
	if (_pfManager->OpenFile(APPEND_TABLE_SUFFIX(tableName), handle) != 0)
	{
		cerr << "RM::reorganizeTable - Failed to open data file [" << tableName << "]." << endl;
		return -1;
	}

	RC rc = this->reoranizeTBL(handle);

	if (rc != 0)
	{
		cerr << "RM::reorganizeTable - Failed to re-organize all the tuples in table [" << tableName << "]." << endl;
		return -2;
	}

	// need to close the file anyway
	if (_pfManager->CloseFile(handle) != 0)
	{
		cerr << "RM::reorganizeTable - Failed to close data file [" << tableName << "]." << endl;
		return -1;
	}

	cout << "RM::reorganizeTable - Successfully re-organized all the tuples in table [" << tableName << "]." << endl;
	return 0;
}

/*********************** HELPER FUNCTIONS ***********************/

RC RM::initTableAttributes(PF_FileHandle &handle, const char *tableName, const vector<Attribute> &attrs)
{
	cout << "Initialize attributes for table [" << tableName << "]." << endl;
	for (unsigned index = 0; index < attrs.size(); index++)
	{
		Attribute attr = attrs[index];
		char record[100];
		unsigned size = prepareAttribute(record, tableName, attr, index + 1);

		RID rid;
		this->insert(handle, record, size, rid);
	}

	return 0;
}

/**
 * Checks if the record to which slotNum points contains an attribute in given table.
 *
 * @return position of given attribute in given table (0 indicates the attribute is not in the table).
 */
unsigned attrInTable(const void *page, const Slot *directory, const unsigned slotNum, const char *tableName, Attribute &attr)
{
	Slot slot = directory[slotNum];
	if (!slot.flag)
		return 0;

	char record[slot.length];
	memcpy(record, (char *)page + slot.offset, slot.length);

	int len = 0;
	unsigned offset = 4;	// skip field count

	// table name
	memcpy(&len, record + offset, 4);
	offset += 4;
	char tname[len];
	memcpy(tname, record + offset, len);
	tname[len] = '\0';	// IMPORTANT
	if (memcmp(tname, tableName, len) != 0)
		return 0;

	// attribute name
	offset += len;
	memcpy(&len, record + offset, 4);
	offset += 4;
	char aname[len];
	memcpy(aname, record + offset, len);
	aname[len] = '\0';	// IMPORTANT
	attr.name = aname;

	// attribute type
	offset += len;
	memcpy(&attr.type, record + offset, 4);

	// attribute length
	offset += 4;
	memcpy(&attr.length, record + offset, 4);

	// attribute position
	unsigned pos = 0;
	offset += 4;
	memcpy(&pos, record + offset, 4);

	cout << "RM::attrInTable - Got attribute at position " << pos << ": " << attr.name << "|" << attr.type << "|" << attr.length << endl;
	return pos;
}

/**
 * Loads attributes according given table name.
 */
RC RM::loadAttributes(PF_FileHandle &handle, const char *tableName, vector<Attribute> &attrs)
{
	cout << "RM::loadAttributes - Loading attributes for table [" << tableName << "]." << endl;
	for (unsigned index = 0; index < _attrsCache.size(); index++)
	{
		if (strcmp(_attrsCache[index].tableName.c_str(), tableName) == 0)
		{
			attrs = _attrsCache[index].attrs;
			cout << "RM::loadAttributes - Loaded attributes from cache." << endl;
			return 0;
		}
	}

	unsigned totalPageNum = handle.GetNumberOfPages();
	unsigned count = 0;
	Slot directory[SLOTS_CAPACITY];
	void *page = malloc(PF_PAGE_SIZE);

	for (unsigned pageNum = 0; pageNum < totalPageNum; pageNum++)
	{
		handle.ReadPage(pageNum, page);
		count = readDirectory(page, directory);

		Attribute sortedAttrs[50];	// TODO: update the capacity
		unsigned short attrCount = 0;
		for (unsigned index = 0; index < count; index++)
		{
			Attribute attr;
			unsigned pos = attrInTable(page, directory, index, tableName, attr);
			if (pos)
			{
				sortedAttrs[pos] = attr;
				attrCount++;
			}
		}
		for (unsigned short index = 1; index <= attrCount; index++)
			attrs.push_back(sortedAttrs[index]);
	}

	// update attributes cache
	AttributeCache attrCache;
	attrCache.tableName = tableName;
	attrCache.attrs = attrs;
	_attrsCache.push_back(attrCache);
	cout << "RM::loadAttributes - Loaded attributes from catalog and updated attributes cache." << endl;

	free(page);
	return 0;
}

RC RM::markDeletion(PF_FileHandle &handle, const char *tableName)
{
	unsigned totalPageNum = handle.GetNumberOfPages();
	unsigned count = 0;
	Slot directory[SLOTS_CAPACITY];
	void *page = malloc(PF_PAGE_SIZE);

	for (unsigned pageNum = 0; pageNum < totalPageNum; pageNum++)
	{
		handle.ReadPage(pageNum, page);
		count = readDirectory(page, directory);

		for (unsigned index = 0; index < count; index++)
		{
			Attribute attr;
			if (attrInTable(page, directory, index, tableName, attr))
			{
				RID rid;
				rid.pageNum = pageNum;
				rid.slotNum = index;
				this->deleteTuple(CATALOG_TABLE_NAME, rid);
			}
		}
	}

	// update catalog cache
	for (unsigned index = 0; index < _attrsCache.size(); index++)
		if (strcmp(_attrsCache[index].tableName.c_str(), tableName) == 0)
			_attrsCache.erase(_attrsCache.begin() + index);

	return 0;
}

RC RM::insert(PF_FileHandle &handle, const void *data, const unsigned size, RID &rid)
{
	void *page = malloc(PF_PAGE_SIZE);
	unsigned totalPageNum = handle.GetNumberOfPages();
	Slot slots[SLOTS_CAPACITY];
	unsigned count = 0;
	unsigned pageNum = 0;
	int slotNum = -1;

	// try to find existing slot with enough free space
	while (pageNum < totalPageNum)
	{
		handle.ReadPage(pageNum, page);
		count = readDirectory(page, slots);

		slotNum = findSpace(slots, count, size);
		if (slotNum >= 0)	// found enough space for the data on current page
		{
			cout << "RM::insert - Found space at [" << pageNum << ":" << slotNum << "]" << endl;
			break;
		}

		pageNum++;
		slotNum = -1;
	}

	// TODO: TEST add new data and overflow to the next one
	// no existing slot with enough space available; insert data on a new page
	if (pageNum == totalPageNum)
	{
		page = realloc(page, PF_PAGE_SIZE);
		init(slots);
		count = 0;
		slotNum = 0;
		cout << "RM::insert - New page " << pageNum << "." << endl;
	}

	updatePageData(page, data, slots[slotNum].offset, size);
	updateDirectory(slots, count, slotNum, size);
	writeDirectory(page, slots, count);
	handle.WritePage(pageNum, page);
	rid.pageNum = pageNum;
	rid.slotNum = slotNum;

	free(page);
	return 0;
}

/**
 * Locates data with given rid and updates the rid to point to ultimate slot containing data.
 */
RC locateData(PF_FileHandle &handle, RID &rid, void *page, Slot *directory, unsigned &count)
{
	unsigned short flag = 2;
	while (flag != 0 && flag != 1)		// found ultimate slot when flag is 0 or 1
	{
		RC rc = handle.ReadPage(rid.pageNum, page);
		if (rc != 0)
		{
			cerr << "RM::locateData - Failed to read page at page number [" << rid.pageNum << "]." << endl;
			return -1;
		}

		count = readDirectory(page, directory);
		if (rid.slotNum >= count)
		{
			cerr << "RM::locateData - Invalid slot number [" << rid.slotNum << "]." << endl;
			return -2;
		}

		flag = directory[rid.slotNum].flag;
		if (flag == 3)
		{
			cerr << "RM::locateData - Invalid slot number [" << rid.slotNum << "]." << endl;
			return -2;
		}
		else if (flag == 2)		// tomb stone
		{
			memcpy(&rid.pageNum, (char *)page + directory[rid.slotNum].offset, 4);
			memcpy(&rid.slotNum, (char *)page + directory[rid.slotNum].offset + 4, 4);
			cout << "RM::locateData - Found tomb stone pointing to [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
		}
	}
	cout << "RM::locateData - Found data at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;

	return 0;
}

/**
 * Reads data specified by rid.
 *
 * @para rid will be updated to the one ultimately points to real data
 * 			if the original one points to a tomb stone.
 */
RC RM::read(PF_FileHandle &handle, const RID &rid, void *data, unsigned &size)
{
	void *page = malloc(PF_PAGE_SIZE);

	RID idCopy(rid);
	Slot directory[SLOTS_CAPACITY];
	unsigned count;
	RC rc = locateData(handle, idCopy, page, directory, count);
	if (rc != 0)
		return rc;

	if (directory[idCopy.slotNum].flag == 0)
	{
		cerr << "RM::read - The data at given rid [" << rid.pageNum << ":" << rid.slotNum << "] have been deleted." << endl;
		return -1;
	}

	size = directory[idCopy.slotNum].length;
	memcpy(data, (char *)page + directory[idCopy.slotNum].offset, size);
	return 0;
}

RC RM::update(PF_FileHandle &handle, const void *data, const unsigned size, const RID &rid)
{
	RID idCopy(rid);
	void *page = malloc(PF_PAGE_SIZE);
	Slot directory[SLOTS_CAPACITY];
	unsigned count;
	RC rc = locateData(handle, idCopy, page, directory, count);

	if (rc != 0)
	{
		cerr << "RM::update - Failed to find record at given RID [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
		return -1;
	}

	if (directory[rid.slotNum].flag == 0)
	{
		cerr << "RM::update - The data at given rid [" << rid.pageNum << ":" << rid.slotNum << "] have been deleted; no further action." << endl;
		return -2;
	}

	int discrepancy = size - directory[idCopy.slotNum].length;

	// the space of original slot is enough
	if (discrepancy <= 0)
	{
		memcpy((char *)page + directory[idCopy.slotNum].offset, data, size);

		updateDirectory(directory, count, idCopy.slotNum, size);
		writeDirectory(page, directory, count);
		handle.WritePage(idCopy.pageNum, page);
		cout << "RM::update - Updated tuple on its own slot." << endl;
		return 0;
	}

	// TODO: MEDIUM!! If there is enough space followed to original slot (slot id is not necessarily adjacent),
	// 			just consolidate slots to put new data.
	// enough space on current page for shift of records
	if (directory[count].length + PAGE_PRESCRIBED_FREE_SPACE >= discrepancy)
	{
		unsigned short restDataOffset = directory[idCopy.slotNum].offset + directory[idCopy.slotNum].length;
		unsigned short restDataSize = directory[count].offset - restDataOffset;
		char updateData[size + restDataSize];
		memcpy(updateData, data, size);
		memcpy(updateData + size, (char *)page + restDataOffset, restDataSize);
		memcpy((char *)page + directory[idCopy.slotNum].offset, updateData, size + restDataSize);

		updateDirectory(directory, count, idCopy.slotNum, size);
		writeDirectory(page, directory, count);
		handle.WritePage(idCopy.pageNum, page);

		cout << "RM::update - Updated tuple on its own slot by shifting data backward on the page." << endl;
		return 0;
	}

	// need to find new slot and insert data
	RID newRid;
	this->insert(handle, data, size, newRid);
	cout << "RM::update - No enough space; inserted the data at [" << newRid.pageNum << ":" << newRid.slotNum << "]." << endl;

	// update tomb stone	// TODO: MEDIUM!! TEST
	char tuple[8];
	memcpy(tuple, &newRid.pageNum, 4);
	memcpy(tuple + 4, &newRid.slotNum, 4);
	memcpy((char *)page + directory[idCopy.slotNum].offset, tuple, directory[idCopy.slotNum].length);
	directory[idCopy.slotNum].flag = 2;
	directory[idCopy.slotNum].length = 8;
	// release extra space
	Slot freeSpace;
	freeSpace.flag = 0;
	freeSpace.offset = directory[idCopy.slotNum].offset + 8;
	freeSpace.length = directory[idCopy.slotNum].length - 8;
	directory[count + 1] = directory[count];
	directory[count++] = freeSpace;
	// update data
	updateDirectory(directory, count, idCopy.slotNum, size);
	writeDirectory(page, directory, count);
	handle.WritePage(idCopy.pageNum, page);
	cout << "RM::update - Updated tomb stone at [" << idCopy.pageNum << ":" << idCopy.slotNum << "]." << endl;

	return 0;
}

RC RM::remove(PF_FileHandle &handle, const RID &rid)
{
	RID idCopy(rid);
	void *page = malloc(PF_PAGE_SIZE);
	Slot directory[SLOTS_CAPACITY];
	unsigned count;
	RC rc = locateData(handle, idCopy, page, directory, count);

	if (rc != 0)
	{
		cerr << "RM::update - Failed to find record at given RID [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
		return -1;
	}

	if (directory[rid.slotNum].flag == 0)
	{
		cerr << "RM::update - The data at given rid [" << rid.pageNum << ":" << rid.slotNum << "] have been deleted; no further action." << endl;
		return -2;
	}

	// delete data by setting flag to 0
	directory[idCopy.slotNum].flag = 0;
	writeDirectory(page, directory, count);
	handle.WritePage(idCopy.pageNum, page);

	return 0;
}

RC RM::clearTuples(PF_FileHandle &handle)
{
	void *page = malloc(PF_PAGE_SIZE);
	unsigned totalPageNum = handle.GetNumberOfPages();
	Slot slots[SLOTS_CAPACITY];
	unsigned count = 0;
	for (unsigned pageNum = 0; pageNum < totalPageNum; pageNum++)
	{
		handle.ReadPage(pageNum, page);
		count = readDirectory(page, slots);
		for (unsigned slotNum = 0; slotNum < count; slotNum++)
			slots[slotNum].flag = 0;

		writeDirectory(page, slots, count);
		handle.WritePage(pageNum, page);
	}

	return 0;
}

RC RM::reorganizePG(PF_FileHandle &handle, const unsigned pageNumber)
{
	char *page = (char *)malloc(PF_PAGE_SIZE);
	char *newPage = (char *)malloc(PF_PAGE_SIZE);
	Slot slots[SLOTS_CAPACITY];
	unsigned count = 0;
	handle.ReadPage(pageNumber, page);
	count = readDirectory(page, slots);

	unsigned short offset = 0;
	for (unsigned slotNum = 0; slotNum < count; slotNum++)
	{
		if (slots[slotNum].flag == 0)
		{
			slots[slotNum].flag = 3;
			slots[count].length += slots[slotNum].length;
		}
		else if (slots[slotNum].flag != 3)
		{
			memcpy(newPage + offset, page + slots[slotNum].offset, slots[slotNum].length);
			slots[slotNum].offset = offset;
			offset += slots[slotNum].length;
		}
	}
	slots[count].offset = offset;
	writeDirectory(page, slots, count);
	handle.WritePage(pageNumber, page);

	return 0;
}

void loadPages(PF_FileHandle &handle, const unsigned start, void **pagesIn, const unsigned pageCount)
{
	for (unsigned pageNum = 0; pageNum < pageCount; pageNum++)
	{
		pagesIn[pageNum] = malloc(PF_PAGE_SIZE);
		handle.ReadPage(start, pagesIn[pageNum]);
	}
}

void writePages(PF_FileHandle &handle, const unsigned start, void **pagesOut, const unsigned pageCount)
{
	for (unsigned pageNum = 0; pageNum < pageCount; pageNum++)
		handle.WritePage(start + pageNum, pagesOut[pageNum]);
}

RC RM::reoranizeTBL(PF_FileHandle &handle)
{
	unsigned totalPageNum = handle.GetNumberOfPages();
	unsigned loads = totalPageNum / REORGANIZE_LOAD + 1;
	unsigned startOut = 0;

	void *pagesOut[REORGANIZE_LOAD];
	unsigned pageOutNum = 0;
	Slot slotsOut[SLOTS_CAPACITY];
	init(slotsOut);
	unsigned slotOutNum = 0;

	for (unsigned load = 0; load < loads; load++)	// loop if there are pages left
	{
		unsigned pageInCount = totalPageNum / REORGANIZE_LOAD > load ?
				REORGANIZE_LOAD : totalPageNum % REORGANIZE_LOAD;
		void *pagesIn[pageInCount];
		unsigned startIn = REORGANIZE_LOAD * load;
		loadPages(handle, startIn, pagesIn, pageInCount);	// load a batch of pages
		cout << "RM::reoranizeTBL - Loaded " << pageInCount << " page(s) from the table." << endl;

		Slot slotsIn[SLOTS_CAPACITY];
		unsigned slotInCount = 0;
		RID rid;	// the cursor of input records
		for (unsigned pageInNum = 0; pageInNum < pageInCount; pageInNum++)	// read input pages one by one
		{
			rid.pageNum = startIn + pageInNum;
			void *pageIn = pagesIn[pageInNum];
			slotInCount = readDirectory(pageIn, slotsIn);
			for (rid.slotNum = 0; rid.slotNum < slotInCount; rid.slotNum++)	// read input slots one by one
			{
				if (slotsIn[rid.slotNum].flag == 1)		// the slot contains real data
				{
					cout << "RM::reoranizeTBL - Got one valid slot at [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
					if (slotsOut[slotOutNum].length < slotsIn[rid.slotNum].length + EACH_SLOT_INFO_LEN)	// new page is full
					{
						cout << "RM::reoranizeTBL - One new page is full." << endl;
						pageOutNum++;
						if (pageOutNum == REORGANIZE_LOAD)	// if out pages reach REORGANIZE_LOAD, write them to file
						{
							cout << "RM::reoranizeTBL - Read re-organization load; writing pages to file." << endl;
							writePages(handle, startOut, pagesOut, REORGANIZE_LOAD);
							startOut += REORGANIZE_LOAD;		// update cursor for writing

							// renew out pages
							for (unsigned pageNum = 0; pageNum < REORGANIZE_LOAD; pageNum++)
								free(pagesOut[pageNum]);
							pageOutNum = 0;
						}
					}

					// initialize a new out page
					pagesOut[pageOutNum] = malloc(PF_PAGE_SIZE);
					init(slotsOut);
					slotOutNum = 0;
					cout << "RM::reoranizeTBL - Initialized one new page for re-organization." << endl;

					// write one slot to out page
					memcpy((char *)pagesOut[pageOutNum] + slotsOut[slotOutNum].offset,
							(char *)pageIn + slotsIn[rid.slotNum].offset, slotsIn[rid.slotNum].length);
					// update directory for out page
					Slot empty = slotsOut[slotOutNum];
					empty.offset += slotsIn[rid.slotNum].length;
					empty.length -= slotsIn[rid.slotNum].length;
					slotsOut[slotOutNum] = slotsIn[rid.slotNum];
					slotOutNum++;
					slotsOut[slotOutNum] = empty;
					writeDirectory(pagesOut[pageOutNum], slotsOut, slotOutNum);
					cout << "RM::reoranizeTBL - Wrote one valid slot to new page at [" << startOut + pageOutNum << ":" << slotOutNum << "] and updated directory." << endl;
				}
			}
		}

		for (unsigned pageNum = 0; pageNum < pageInCount; pageNum++)
			free(pagesIn[pageNum]);
	}

	// write left out pages if necessary
	unsigned leftPageCount = slotOutNum == 0 ? pageOutNum : pageOutNum + 1;
	if (leftPageCount > 0)
	{
		cout << "RM::reoranizeTBL - Writing left " << leftPageCount << " page(s) to file." << endl;
		writePages(handle, startOut, pagesOut, leftPageCount);
		for (unsigned pageNum = 0; pageNum < leftPageCount; pageNum++)
			free(pagesOut[pageNum]);
	}

	// clean extra pages in the original file if necessary
	unsigned currentTotalPages = startOut + leftPageCount;
	if (currentTotalPages < totalPageNum)
	{
		cout << "RM::reoranizeTBL - " << (totalPageNum - currentTotalPages) << " extra page(s) in the original file; clean them." << endl;
		for (unsigned index = currentTotalPages; index < totalPageNum; index++)
		{
			void *page = malloc(PF_PAGE_SIZE);
			handle.ReadPage(index, page);
			Slot slots[SLOTS_CAPACITY];
			init(slots);
			writeDirectory(page, slots, 0);
			handle.WritePage(index, page);
			free(page);
		}
	}

	return 0;
}

RC RM::appendAttribute(PF_FileHandle &handle, const char *tableName, const Attribute attr)
{
	vector<Attribute> attrs;
	this->loadAttributes(handle, tableName, attrs);

	// check attribute name
	for (unsigned short index = 0; index < attrs.size(); index++)
	{
		if (strcmp(attr.name.c_str(), attrs[index].name.c_str()) == 0)
		{
			cerr << "RM::appendAttribute - An attribute named [" << attr.name << "] ";
			cerr << "has already existed in table [" << tableName << "]." << endl;
			return -1;
		}
	}

	char record[100];	// TODO: update it
	unsigned size = prepareAttribute(record, tableName, attr, attrs.size() + 1);
	RID rid;
	this->insert(handle, record, size, rid);

	// update catalog cache
	for (unsigned index = 0; index < _attrsCache.size(); index++)
	{
		if (strcmp(_attrsCache[index].tableName.c_str(), tableName) == 0)
		{
			_attrsCache[index].attrs.push_back(attr);
			cout << "RM::appendAttribute - updated attributes in cache for table [" << tableName << "]." << endl;
		}
	}

	return 0;
}

/************************ ScanIterator ************************/

template <class T>
bool comp(const T a, const CompOp &compOp, const T b)
{
	switch (compOp)
	{
	case CompOp(0):
			return a == b;
	case CompOp(1):
			return a < b;
	case CompOp(2):
			return a > b;
	case CompOp(3):
			return a <= b;
	case CompOp(4):
			return a >= b;
	case CompOp(5):
			return a != b;
	case CompOp(6):
			return false;
	}
	return false;
}

bool compare(const void *recordValue, const CompOp &compOp, const void *inValue, const AttrType &type)
{
	if (type == AttrType(0))
	{
		int iRecord = *(int *)recordValue;
		int iIn = *(int *)inValue;
		return comp(iRecord, compOp, iIn);
	}
	else if (type == AttrType(1))
	{
		float fRecord = *(float *)recordValue;
		float fIn = *(float *)inValue;
		return comp(fRecord, compOp, fIn);
	}
	else
	{
		string sRecord = (char *)recordValue;
		string sIn = (char *)inValue;
		return comp(sRecord, compOp, sIn);
	}
}

AttrType attrTypeof(const char *attrName, const vector<Attribute> &attrs)
{
	for (unsigned index = 0; index < attrs.size(); index++)
		if (strcmp(attrName, attrs[index].name.c_str()) == 0)
			return attrs[index].type;
	return AttrType(0);		// TODO LOW! not good
}

void projectTuple(const void *tuple, const vector<Attribute> attrs, const vector<string> &attrNames, void *data)
{
	cout << "Helper::projectTuple - Projecting data..." << endl;
	char recordValue[100];
	unsigned offset = 0;

	for (unsigned short index = 0; index < attrNames.size(); index++)
	{
		cout << "Helper::projectTuple - For attribute: " << attrNames[index] << endl;
		unsigned len = 4;
		getAttributeValue(tuple, attrs, attrNames[index].c_str(), recordValue);
		if (attrTypeof(attrNames[index].c_str(), attrs) == AttrType(2))
		{
			memcpy(&len, recordValue, 4);
			len += 4;
		}
		memcpy((char *)data + offset, recordValue, len);
		offset += len;
	}
}

RM_ScanIterator::RM_ScanIterator()
{
	this->_reading = false;
	this->_value = NULL;
}
RM_ScanIterator::~RM_ScanIterator()
{
	if (this->_value)
		free(this->_value);
}

RC RM_ScanIterator::startReading()
{
	if (this->_reading)
	{
		cerr << "RM_ScanIterator::setTableName - Table name cannot be changed before the iterator is closed." << endl;
		return -1;
	}

	this->_pageNum = 0;
	this->_slotNum = -1;
	this->_reading = true;

	return 0;
}

void RM_ScanIterator::setTableName(const string tableName)
{
	if (this->_reading)
	{
		cerr << "RM_ScanIterator::setTableName - Table name cannot be changed before the iterator is closed." << endl;
		return;
	}
	this->_tableName = tableName;
}

void RM_ScanIterator::setCondAttr(const string condAttribute)
{
	if (this->_reading)
	{
		cerr << "RM_ScanIterator::setTableName - Condition attribute cannot be changed before the iterator is closed." << endl;
		return;
	}
	this->_conditionAttribute = condAttribute;
}

void RM_ScanIterator::setCompOp(const CompOp compOp)
{
	if (this->_reading)
	{
		cerr << "RM_ScanIterator::setCompOp - CompOp cannot be changed before the iterator is closed." << endl;
		return;
	}
	this->_compOp = compOp;
}

void RM_ScanIterator::setValue(const void *value)
{
	if (this->_reading)
	{
		cerr << "RM_ScanIterator::setValue - Value cannot be changed before the iterator is closed." << endl;
		return;
	}
	if (this->_value)
		free(this->_value);
	this->_value = malloc(PF_PAGE_SIZE);
	strcpy((char* )this->_value, (char *)value);
}

void RM_ScanIterator::setAttrNames(const vector<string> &attrNames)
{
	if (this->_reading)
	{
		cerr << "RM_ScanIterator::setAttrNames - Attribute names cannot be changed before the iterator is closed." << endl;
		return;
	}
	this->_attributeNames = attrNames;
}

// "data" follows the same format as RM::insertTuple()
RC RM_ScanIterator::getNextTuple(RID &rid, void *data)
{
	cout << "RM_ScanIterator::getNextTuple - Getting next tuple..." << endl;

	RM *rm = RM::Instance();
	vector<Attribute> attrs;
	rm->getAttributes(this->_tableName, attrs);
	AttrType type = attrTypeof(this->_conditionAttribute.c_str(), attrs);

	PF_Manager *manager = PF_Manager::Instance();
	PF_FileHandle handle;
	if (manager->OpenFile(APPEND_TABLE_SUFFIX(this->_tableName), handle) != 0)
	{
		cerr << "RM::readTuple - Failed to open data file [" << this->_tableName << "]." << endl;
		return -1;
	}

	void *page = malloc(PF_PAGE_SIZE);
	Slot slots[SLOTS_CAPACITY];
	unsigned count = 0;

	unsigned totalPageNum = handle.GetNumberOfPages();
	unsigned pageNum = this->_pageNum;
	while (pageNum < totalPageNum)
	{
		this->_pageNum = pageNum;
		handle.ReadPage(pageNum, page);
		count = readDirectory(page, slots);
		unsigned slotNum = this->_slotNum + 1;
		while (slotNum < count)
		{
			void *tuple = malloc(PF_PAGE_SIZE);	// TODO: update it!
			void *recordValue = malloc(PF_PAGE_SIZE);
			cout << "RM_ScanIterator::getNextTuple - Analyzing slot [" << pageNum << ":" << slotNum << "]." << endl;
			RID idCopy;
			idCopy.pageNum = pageNum;
			idCopy.slotNum = slotNum;
			if (locateData(handle, idCopy, page, slots, count) != 0 ||
					slots[idCopy.slotNum].flag == 0)
			{
				cout << "RM_ScanIterator::getNextTuple - Invalid data at [" << pageNum << ":" << slotNum << "]; skipping." << endl;
				slotNum++;
				continue;
			}
			memcpy(tuple, (char *)page + slots[idCopy.slotNum].offset, slots[idCopy.slotNum].length);

			if (this->_compOp != CompOp(6))
			{
				cout << "RM_ScanIterator::getNextTuple - Performing comparison CompOp: " << this->_compOp << " on attribute [" << this->_conditionAttribute << "]." << endl;
				getAttributeValue(tuple, attrs, this->_conditionAttribute.c_str(), recordValue);

				if (this->_value != NULL)
				{
					if (type == AttrType(2))
					{
						unsigned len = 0;
						char end = '\0';

						memcpy(&len, recordValue, 4);
						memcpy(recordValue, (char *)recordValue + 4, len);
						memcpy((char *)recordValue + 4 + len, &end, 1);

						memcpy(&len, this->_value, 4);
						memcpy(this->_value, (char *)this->_value + 4, len);
						memcpy((char *)this->_value + 4 + len, &end, 1);
					}

					if (!compare(recordValue, this->_compOp, this->_value, type))
					{
						cout << "RM_ScanIterator::getNextTuple - Data at [" << pageNum << ":" << slotNum << "] does not meet criterion." << endl;
						slotNum++;
						free(recordValue);
						free(tuple);
						continue;
					}
				}
			}

			cout << "RM_ScanIterator::getNextTuple - Found one valid record at [" << pageNum << ":" << slotNum << "]." << endl;
			this->_slotNum = slotNum;
			projectTuple(tuple, attrs, this->_attributeNames, data);
			free(recordValue);
			free(tuple);
			break;
		}

		if (slotNum < count)
			break;
		pageNum++;
	}

	if (manager->CloseFile(handle) != 0)
	{
		cerr << "RM::readTuple - Failed to close data file [" << this->_tableName << "]." << endl;
		return -1;
	}

	if (pageNum >= totalPageNum)
	{
		cout << "RM_ScanIterator::getNextTuple - Reach the end of table." << endl;
		return RM_EOF;
	}

	rid.pageNum = this->_pageNum;
	rid.slotNum = this->_slotNum;
	return 0;
}

RC RM_ScanIterator::close()
{
	this->_tableName = "";
	this->_conditionAttribute = "";
	this->_compOp = CompOp(0);
	this->_attributeNames = vector<string>(0);
	this->_value = NULL;
	this->_reading = false;

	return 0;
}
