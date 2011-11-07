#include "pf.h"
#include "stdio.h"
#include <iostream>
#include <stddef.h>

PF_Manager* PF_Manager::_pf_manager = 0;

PF_Manager* PF_Manager::Instance()
{
    if(!_pf_manager)
        _pf_manager = new PF_Manager;
    
    return _pf_manager;    
}


PF_Manager::PF_Manager()
{
}


PF_Manager::~PF_Manager()
{
	PF_Manager::_pf_manager = 0;
}

    
RC PF_Manager::CreateFile(const char *fileName)
{
	FILE * file = fopen(fileName, "r");
	if (file != NULL) {
		fclose(file);
		cerr << "Failed to create file \"" << fileName << "\" since a file with the name has already existed." << endl;
	    return -1;
	}

	file = fopen(fileName, "w");

	if (file != NULL)
	{
		fclose (file);
		return 0;
	}
	else
	{
		cerr << "Failed to create file \"" << fileName << "\"." << endl;
	    return -2;
	}
}


RC PF_Manager::DestroyFile(const char *fileName)
{
	if (remove(fileName) != 0)
	{
		cerr << "Cannot remove file \"" << fileName << "\"." << endl;
		return -1;
	}
    return 0;
}


RC PF_Manager::OpenFile(const char *fileName, PF_FileHandle &fileHandle)
{
	if (&fileHandle == NULL)
	{
		cerr << "Cannot open file while PF_FileHandle is NULL." << endl;
		return -2;
	}

	FILE * file = fopen(fileName, "rb+");
	if (file == NULL)
	{
		cerr << "Failed to open file \"" << fileName << "\"." << endl;
	    return -1;
	}

	if (fileHandle._file != NULL)
	{
		cerr << "Cannot open file \"" << fileName << "\" with the given PF_FileHandle, since it is for another file." << endl;
		return -3;
	}

	fileHandle._file = file;
	return 0;
}


RC PF_Manager::CloseFile(PF_FileHandle &fileHandle)
{
	if (&fileHandle == NULL)
	{
		cout << "No file needs to be closed." << endl;
	}

	if (fileHandle._file != NULL)
	{
		if (fclose(fileHandle._file) != 0)
		{
			cerr << "Failed to close file." << endl;
			return -1;
		}
		fileHandle._file = NULL;
	}
    return 0;
}


PF_FileHandle::PF_FileHandle()
{
	this->_file = NULL;
}
 

PF_FileHandle::~PF_FileHandle()
{
	delete this->_file;
}

/*
 * Reads page from specified pageNum, which starts from 0.
 */
RC PF_FileHandle::ReadPage(PageNum pageNum, void *data)
{
	if (this->_file == NULL)
	{
		cerr << "No file to operate." << endl;
		return -1;
	}

	fseek (this->_file, 0, SEEK_END);
	long lSize = ftell (this->_file);
	rewind (this->_file);

	if (PF_PAGE_SIZE * pageNum >= lSize)	// no data to read starting from lSize
	{
		cerr << "Page " << pageNum << " does not exist." << endl;
		return -2;
	}

	fseek(this->_file, PF_PAGE_SIZE * pageNum, SEEK_SET);
	fread(data, 1, PF_PAGE_SIZE, this->_file);
    return 0;
}

/*
 * Writes page into specified pageNum, which starts from 0.
 * If the given pageNum is currentTotalPageNum + 1, it writes as AppendPage;
 * otherwise, return error code -2.
 */
RC PF_FileHandle::WritePage(PageNum pageNum, const void *data)
{
	if (this->_file == NULL)
	{
		cerr << "No file to operate." << endl;
		return -1;
	}

	fseek(this->_file, 0, SEEK_END);
	long lSize = ftell(this->_file);
	rewind(this->_file);

	if (PF_PAGE_SIZE * pageNum > lSize)
	{
		cerr << "Page " << pageNum << " does not exist." << endl;
		return -2;
	}

	fseek(this->_file, PF_PAGE_SIZE * pageNum, SEEK_SET);
	fwrite(data, 1, PF_PAGE_SIZE, this->_file);
	fflush(this->_file);
    return 0;
}


RC PF_FileHandle::AppendPage(const void *data)
{
	if (this->_file == NULL)
	{
		cerr << "No file to operate." << endl;
		return -1;
	}

	fseek(this->_file, 0, SEEK_END);
	fwrite(data, 1, PF_PAGE_SIZE, this->_file);
	fflush(this->_file);
    return 0;
}

// Returns the number of pages in file
unsigned PF_FileHandle::GetNumberOfPages()
{
	if (this->_file == NULL)
	{
		cerr << "No file to operate." << endl;
		return -1;
	}

	fseek (this->_file, 0, SEEK_END);
	long lSize = ftell (this->_file);	// point to EOF, which is (endCharIndex + 1); lSize will be the length of the file

	return lSize >= 0 ? lSize / PF_PAGE_SIZE : 0;
}


