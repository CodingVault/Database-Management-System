#include <fstream>
#include <iostream>
#include <cassert>
#include <stdlib.h>
#include <sys/stat.h>

#include "pf.h"

using namespace std;

const int success = 0;
const string sFileName = "db.data";

PF_Manager * manager = NULL;
PF_FileHandle * handle = NULL;
RC rc = -1;

void Setup()
{
	manager = PF_Manager::Instance();
	handle = new PF_FileHandle();
}

void Teardown()
{
	manager = NULL;
	delete handle;
}

// Check if a file exists
bool FileExists(string fileName)
{
    struct stat stFileInfo;

    if(stat(fileName.c_str(), &stFileInfo) == 0) return true;
    else return false;
}

int PFTest_1(PF_Manager *pf)
{
    // Functions Tested:
    // 1. CreateFile
    cout << "****In PF Test Case 1****" << endl;

    RC rc;
    string fileName = "test";

    // Create a file named "test"
    rc = pf->CreateFile(fileName.c_str());
    assert(rc == success);

    if(FileExists(fileName.c_str()))
    {
        cout << "File " << fileName << " has been created." << endl << endl;
        return 0;
    }
    else
    {
        cout << "Failed to create file!" << endl;
        return -1;
    }

    // Create "test" again, should fail
    rc = pf->CreateFile(fileName.c_str());
    assert(rc != success);

    return 0;
}

int PFTest_2(PF_Manager *pf)
{
    // Functions Tested:
    // 1. OpenFile
    // 2. AppendPage
    // 3. GetNumberOfPages
    // 4. WritePage
    // 5. ReadPage
    // 6. CloseFile
    // 7. DestroyFile
    cout << "****In PF Test Case 2****" << endl;

    RC rc;
    string fileName = "test";

    // Open the file "test"
    PF_FileHandle fileHandle;
    rc = pf->OpenFile(fileName.c_str(), fileHandle);
    assert(rc == success);

    // Append the first page
    // Write ASCII characters from 32 to 125 (inclusive)
    void *data = malloc(PF_PAGE_SIZE);
    for(unsigned i = 0; i < PF_PAGE_SIZE; i++)
    {
        *((char *)data+i) = i % 94 + 32;
    }
    rc = fileHandle.AppendPage(data);
    assert(rc == success);

    // Get the number of pages
    unsigned count = fileHandle.GetNumberOfPages();
    assert(count == (unsigned)1);

    // Update the first page
    // Write ASCII characters from 32 to 41 (inclusive)
    data = malloc(PF_PAGE_SIZE);
    for(unsigned i = 0; i < PF_PAGE_SIZE; i++)
    {
        *((char *)data+i) = i % 10 + 32;
    }
    rc = fileHandle.WritePage(0, data);
    assert(rc == success);

    // Read the page
    void *buffer = malloc(PF_PAGE_SIZE);
    rc = fileHandle.ReadPage(0, buffer);
    assert(rc == success);

    // Check the integrity
    rc = memcmp(data, buffer, PF_PAGE_SIZE);
    assert(rc == success);

    // Close the file "test"
    rc = pf->CloseFile(fileHandle);
    assert(rc == success);

    free(data);
    free(buffer);

    // DestroyFile
    rc = pf->DestroyFile(fileName.c_str());
    assert(rc == success);

    if(!FileExists(fileName.c_str()))
    {
        cout << "File " << fileName << " has been destroyed." << endl;
        cout << "Test Case 2 Passed!" << endl << endl;
        return 0;
    }
    else
    {
        cout << "Failed to destroy file!" << endl;
        return -1;
    }
}

bool FileExists()
{
	FILE * f = fopen(sFileName.c_str(), "r");
	if (f != NULL)
	{
		fclose(f);
		return true;
	}
	return false;
}

void pfTestCreatFile()
{
	// create file
	rc = manager->CreateFile(sFileName.c_str());
	if (rc != 0)
	{
		exit(-1);
	}
	assert(FileExists());
	cout << "Successfully created file \"" << sFileName.c_str() << "\"." << endl;
}

void pfTestRemoveFile()
{
	// delete file
	rc = manager->DestroyFile(sFileName.c_str());
	if (rc != 0)
	{
		cerr << "Cannot destroy file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	else
	{
		assert(!FileExists());
		cout << "Successfully destroyed file \"" << sFileName << "\"." << endl;
	}
}

void pfTestAppendFile(void *input)
{
	// append file
	rc = manager->OpenFile(sFileName.c_str(), *handle);
	if (rc != 0)
	{
		cout << "Failed to open file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	rc = handle->AppendPage(input);
	if (rc != 0)
	{
		cerr << "Failed to write data into file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	else
	{
		cout << "Successfully appended data into file \"" << sFileName << "\"." << endl;
	}
	rc = manager->CloseFile(*handle);
	if (rc != 0)
	{
		cerr << "Failed to close file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
}

void pfTestReadFile(PageNum pageNum, void *oriInput)
{
	void *output;

	// open file
	rc = manager->OpenFile(sFileName.c_str(), *handle);
	if (rc != 0)
	{
		cout << "Failed to open file \"" << sFileName << "\"." << endl;
		exit(-1);
	}

	// read file
	output = malloc(PF_PAGE_SIZE);
	rc = handle->ReadPage(pageNum, output);
	if (rc != 0)
	{
		cerr << "Failed to read data from file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	else
	{
		cout << "Successfully read data from file \"" << sFileName << "\"." << endl;
	}

	// close file
	rc = manager->CloseFile(*handle);
	if (rc == -1)
	{
		cerr << "Failed to close file \"" << sFileName << "\"." << endl;
		exit(-1);
	}

	// check the integrity
	rc = memcmp(oriInput, output, PF_PAGE_SIZE);
	assert(rc == success);
	cout << "The data read from DB file is the same as expected." << endl;

	free(output);
}

void pfTestModifyFile(PageNum pageNum, void *input)
{
	void *output;

	// open file
	rc = manager->OpenFile(sFileName.c_str(), *handle);
	if (rc != 0)
	{
		cout << "Failed to open file \"" << sFileName << "\"." << endl;
		exit(-1);
	}

	// write file
	rc = handle->WritePage(pageNum, input);
	if (rc != 0)
	{
		cerr << "Failed to write data into file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	else
	{
		cout << "Successfully wrote data into file \"" << sFileName << "\"." << endl;
	}

	// read file
	output = malloc(PF_PAGE_SIZE);
	rc = handle->ReadPage(pageNum, output);
	if (rc != 0)
	{
		cerr << "Failed to read data from file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	else
	{
		cout << "Successfully read data from file \"" << sFileName << "\"." << endl;
	}

	// close file
	rc = manager->CloseFile(*handle);
	if (rc != 0)
	{
		cerr << "Failed to close file \"" << sFileName << "\"." << endl;
		exit(-1);
	}

	// check the integrity
	rc = memcmp(input, output, PF_PAGE_SIZE);
	assert(rc == success);
	cout << "The data read from DB file is the same as those written in." << endl;
}

void pfVerifyFilePages(unsigned expectedNum)
{
	// open file
	rc = manager->OpenFile(sFileName.c_str(), *handle);
	if (rc != 0)
	{
		cout << "Failed to open file \"" << sFileName << "\"." << endl;
		exit(-1);
	}

	// assert page numbers
	unsigned count = handle->GetNumberOfPages();
    assert(count == (unsigned) expectedNum);
	cout << "Total number of page: " << count << ". Verified page numbers as expected." << endl;

	// close file
	rc = manager->CloseFile(*handle);
	if (rc != 0)
	{
		cerr << "Failed to close file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
}

void pfTestComplexOperations(void *oriInput, void *upInput)
{
	cout << "...Test suite for complex operations..." << endl;
	pfTestCreatFile();

	pfTestAppendFile(oriInput);
	pfTestReadFile(0, oriInput);
	pfTestAppendFile(oriInput);
	pfTestModifyFile(1, upInput);

	pfTestAppendFile(oriInput);
	pfTestReadFile(0, oriInput);
	pfTestReadFile(1, upInput);
	pfTestReadFile(2, oriInput);

	pfTestRemoveFile();
	cout << "...Test suite end..." << endl;
}

void pfTestAll(void *oriInput, void *upInput)
{
	cout << "...Test suite for all functions..." << endl;
	pfTestCreatFile();

	pfTestAppendFile(oriInput);
	pfVerifyFilePages(1);

	pfTestModifyFile(0, upInput);
	pfVerifyFilePages(1);

	pfTestAppendFile(oriInput);
	pfVerifyFilePages(2);

	pfTestRemoveFile();
	cout << "...Test suite end..." << endl;
}

void ReadFile(PageNum pageNum, void *output)
{
	// open file
	rc = manager->OpenFile(sFileName.c_str(), *handle);
	if (rc != 0)
	{
		cout << "Failed to open file \"" << sFileName << "\"." << endl;
		exit(-1);
	}

	// read file
	rc = handle->ReadPage(pageNum, output);
	if (rc != 0)
	{
		cerr << "Failed to read data from file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
	else
	{
		cout << "Successfully read data from file \"" << sFileName << "\"." << endl;
	}

	// close file
	rc = manager->CloseFile(*handle);
	if (rc == -1)
	{
		cerr << "Failed to close file \"" << sFileName << "\"." << endl;
		exit(-1);
	}
}

int main()
{
	cout << "test..." << endl;
	Setup();

	void *oriInput = malloc(PF_PAGE_SIZE);
	void *output = malloc(PF_PAGE_SIZE);

	pfTestCreatFile();

	int len = 10;
	char s[] = "catalog.dat";
//	*(int *)oriInput = len;
//	strcat((char *)oriInput + 4, s);
	memcpy(oriInput, &len, 4);
	memcpy((char *)oriInput + 4, s, 11);
	cout << "Input: " << *(char *)oriInput << "<<" << endl;
	pfTestAppendFile(oriInput);
	ReadFile(0, output);
	cout << "Output: " << *(int *)output << "<<" << endl;
	cout << "Len: " << strlen((char *)output) << endl;
	int size = 0;
	char str[11];
	memcpy(&size, output, 4);
	memcpy(str, (char *)output + 4, 11);
	cout << "Size: " << size << endl;
	cout << "String: " << str << endl;

	pfTestRemoveFile();

	free(oriInput);
	free(output);

//	void *oriInput = malloc(PF_PAGE_SIZE);
//	for(unsigned i = 0; i < PF_PAGE_SIZE - 5; i++)
//	{
//		*((char *)oriInput + i) = i % 94 + 32;
//	}
//	void *upInput = malloc(PF_PAGE_SIZE);
//	for(unsigned i = 0; i < PF_PAGE_SIZE; i++)
//	{
//		*((char *)upInput + i) = 'a' + i % 26;
//	}
//
//	pfTestComplexOperations(oriInput, upInput);
//
//	pfTestAll(oriInput, upInput);
//
//	free(oriInput);
//	free(upInput);
//
//	PFTest_1(manager);
//	PFTest_2(manager);

	Teardown();
	cout << "OK" << endl;
}
