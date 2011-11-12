#include <fstream>
#include <iostream>
#include <cassert>
#include <vector>
#include <sys/time.h>

#include "rm.h"

#define TABLE_NAME "test"

using namespace std;

const int success = 0;

void IntToChar(const int data_int, char* data_char)
{
	memcpy(data_char,&data_int,4);
}

void CharToInt(int* data_int,const char* data_char)
{
	memcpy(data_int,data_char,4);
}

void RealToChar(const float data_real, char* data_char)
{
	memcpy(data_char,&data_real,4);
}

void CharToReal(float *data_real, const char* data_char)
{
	memcpy(data_real,data_char,4);
}

void CharToShort(short *data_short, const char* data_char)
{
	memcpy(data_short,data_char,2);
}

void ShortToChar(const short data_short, char* data_char)
{
	memcpy(data_char,&data_short,2);
}
void randomlongdata(void*data,vector<Attribute>&attrs,int* length)
{
	unsigned int i = 0 , j = 0;
		unsigned int randomint = 0;
		int offset = 0;
		char str[100] = {0};
		char data_char[4] = "\0";
		*length = 0;
		for(i = 0; i < attrs.size(); i++)
		{
			if(attrs[i].type == TypeInt)
			{
				IntToChar(rand()%1000,data_char);
				memcpy((char*)data + offset,data_char,4);
				offset += 4;
				*length += 4;
			}
			if(attrs[i].type == TypeReal)
			{
				RealToChar((float)(rand()%1000)/1000,data_char);
				memcpy((char*)data + offset,data_char,4);
				offset += 4;
				*length += 4;
			}
			if(attrs[i].type == TypeVarChar)
			{
				randomint = rand()%50+55;
				IntToChar(randomint,data_char);
				memcpy((char*)data + offset,data_char,4);
				offset += 4;
				*length += 4;
				for(j = 0; j < randomint; j++)
				{
					str[j] = (rand()%26) + 97;
				}
				*length += randomint;
				memcpy((char*)data + offset, str,randomint);
				offset += randomint;
			}
		}
}

bool streamcompare(char*data1,char*data2,int length)
{
	int i = 0;
	for(i = 0; i < length; i++)
	{
		if(data1[i]!= data2[i])
		{
			return 0;
		}
	}
	return 1;
}
void randomdata(void*data,vector<Attribute>&attrs,int* length)
{//generate a random data stream according to attrs
	unsigned int i = 0 , j = 0;
	unsigned int randomint = 0;
	int offset = 0;
	char str[100] = {0};
	char data_char[4] = "\0";
	*length = 0;
	for(i = 0; i < attrs.size(); i++)
	{
		if(attrs[i].type == TypeInt)
		{
			IntToChar(rand()%1000,data_char);
			memcpy((char*)data + offset,data_char,4);
			offset += 4;
			(*length) += 4;
		}
		if(attrs[i].type == TypeReal)
		{
			RealToChar((float)(rand()%1000)/1000,data_char);
			memcpy((char*)data + offset,data_char,4);
			offset += 4;
			(*length) += 4;
		}
		if(attrs[i].type == TypeVarChar)
		{
			randomint = rand()%50+1;
			IntToChar(randomint,data_char);
			memcpy((char*)data + offset,data_char,4);
			offset += 4;
			(*length) += 4;
			for(j = 0; j < randomint; j++)
			{
				str[j] = (rand()%26) + 97;
			}
			(*length) += randomint;
			memcpy((char*)data + offset, str,randomint);
			offset += randomint;
		}
	}
}

void data_print(void* data, vector<Attribute>& attrs)
{
//	unsigned int i = 0;
//	int j = 0;
//	int data_int = 0;
//	float data_float = 0;
//	int offset = 0;
//	for(i = 0; i < attrs.size(); i++)
//	{
//		cout<<attrs[i].name<<": ";
//		if(attrs[i].type == TypeInt)
//		{
//			CharToInt(&data_int,(char*)data + offset);
//			cout<<data_int<<" || ";
//			offset += 4;
//		}
//		if(attrs[i].type == TypeReal)
//		{
//			CharToReal(&data_float,(char*)data + offset);
//			cout<<data_float<<" || ";
//			offset += 4;
//		}
//		if(attrs[i].type == TypeVarChar)
//		{
//			CharToInt(&data_int,(char*)data + offset);
//			offset += 4;
//			cout<<data_int<<": ";
//			for(j = 0; j < data_int; j++)
//			{
//				cout<<*((char*)data + offset);
//				offset ++;
//			}
//			cout<<" || ";
//		}
//	}
//	cout<<endl;
}
void mytest()
{
	unsigned int i = 0, j = 0;
		RM *rm1 = RM::Instance();
		std::string tableName1("column_catalog");
		std::string tableName2("table1");
		vector<Attribute> attrs1;
		vector<Attribute> attrs2;
		Attribute temp_attr;
		/***********************************************************************/
		//get the attributes of the column catalog table
		if(!rm1->getAttributes(tableName1,attrs1))
		{
			cout<<"get attributes of column catalog table successfully!!!"<<endl;
			cout<<"there are "<<attrs1.size()<<" attributes in the catalog and they are:"<<endl;
			for(i = 0; i <attrs1.size(); i++)
			{
				cout<<attrs1[i].name<<" with type "<<attrs1[i].type<<endl;
			}
		}
		else
		{
			cout<<"can't get the attributes of the catalog table!"<<endl;
		}
		/***********************************************************************/
		RID rid;
		char data[PF_PAGE_SIZE] = "\0";
		char data_output[PF_PAGE_SIZE] = {0};
		rid.pageNum = 0;
		rid.slotNum = 1;
		int length = 0;

	    for(i = 0; i  < 5; i++)
	    {
	    	temp_attr.length = 0;
	    	temp_attr.name = 'a';
	    	temp_attr.type = TypeInt;
	    	attrs2.push_back(temp_attr);
	    }
	    attrs2[1].name = 'b';
	    attrs2[2].name = 'c';
	    attrs2[3].name = 'd';
	    attrs2[4].name = 'e';

	    attrs2[1].type = TypeInt;
	    attrs2[2].type = TypeReal;
	    attrs2[3].type = TypeVarChar;
	    attrs2[4].type = TypeReal;

	    /**********************************************************************************/
	    //create the table1
	    if(!rm1->createTable(tableName2, attrs2))
	    {
	    	cout<<"create the table successfully!!!"<<endl;
	    }
	    else
	    {
	    	cout<<"can not create the table!!!"<<endl;
	    }
	    /**********************************************************************************/
	    //get the attrubutes of table1
	    attrs2.clear();
		if(!rm1->getAttributes(tableName2,attrs2))
		{
			cout<<"get type of the attributes of "<<tableName2<<" successfully!!!"<<endl;
			cout<<"there are "<<attrs2.size()<<" attributes in the table"<<tableName2<<" and they are:"<<endl;
			for(i = 0; i <attrs2.size(); i++)
			{
				cout<<attrs2[i].name<<" with type "<<attrs2[i].type<<endl;
			}
		}
		else
		{
			cout<<"can not get the attributes of"<<tableName2<<endl;
		}
	    /**********************************************************************************/
		//insert tuples into table1
		unsigned int tempNum = 200;
		for(i = 0; i < tempNum; i++)
		{
			randomdata(data,attrs2,&length);
			if(!rm1->insertTuple(tableName2,data,rid))
			{
				cout<<"insert tuple in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
			}
			else
			{
				cout<<"can't insert tuple, ERROR!!!!"<< endl;
			}
		}
		/**********************************************************************************/
		//delete tuples
        tempNum = 10;
		unsigned int* slot_no = (unsigned int*)malloc(tempNum*sizeof(unsigned int));
		for(i = 0; i < tempNum; i++)
		{
			slot_no[i] = 2*i;
		}
		for(i = 0; i < 3; i++)
		{
			rid.pageNum = i;
			for(j = 0; j < tempNum; j++)
			{
				rid.slotNum = slot_no[j];
			    if(!rm1->deleteTuple(tableName2,rid))
			    {
				    cout<<"successfully delete the tuple in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
			    }
			    else
			    {
	                cout<<"can't delete the the tuple in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
			    }
			}//for
		}//for
		/*********************************************************************************************/
		//read the tuples in table1
		cout<<"reading"<<endl;
		for(i = 0; i < 3; i++)
		{
			for(j = 0; j < 50; j++)
			{
			    rid.pageNum = i;
			    rid.slotNum = j;
			    if(!rm1->readTuple(tableName2,rid,data))
			    {
				    cout<<"read the tuple successfully! in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;;
			    }
			    else
			    {
				    cout<<"can't find the tuple in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;;
			    }
			}
		}

	    /**************************************************************************************/
		//insert new tuples into table1 after deletion
		for(i = 0; i < 20; i++)
		{
			randomdata(data,attrs2,&length);
			if(!rm1->insertTuple(tableName2,data,rid))
			{
				cout<<"successfully insert the tuples in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
			}
			else
			{
		        cout<<"insertion error!!!"<<endl;
			}
			if(!rm1->readTuple(tableName2,rid,data_output))
			{
				if(streamcompare(data,data_output,length))
		    	{
					cout<<"read the tuple successfully! in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
		    	}
		    	else
		    	{
		    		cout<<"output tuple ERROR!!!"<<endl;
				}
			}
		    else
		    {
			   	cout<<"read tuple ERROR!!!"<<endl;
		    }
		}
		/*********************************************************************************/
		//update the tuples
		tempNum = 10;
		for(i = 0; i < 10; i++)
		{
			rid.pageNum = 1;
			for(j = 0; j < tempNum; j++)
			{
			    rid.slotNum = j;
		        randomlongdata(data,attrs2,&length);
		        if(!rm1->updateTuple(tableName2,data,rid))
		        {
		    	    cout<<"update successfully! for page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
		        }
		        else
		        {
		    	    cout<<"update error! for page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
		         }
		        if(!rm1->readTuple(tableName2,rid,data_output))
		        {
		    	    if(streamcompare(data,data_output,length))
		    	    {
		    		    cout<<"read the tuple successfully! in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
		    	    }
		    	    else
		    	    {
		    		    cout<<"output the tuple error!!!"<<endl;
		    	    }
		        }
		        else
		        {
		           	cout<<"can't read the tuple"<<endl;
		   	    }
		    }
		}
		/*********************************************************************/
		//add an attribute
		Attribute temp_attr2;
		temp_attr2.length = 0;
		temp_attr2.name = "f";
		temp_attr2.type = TypeVarChar;
	    if(!rm1->addAttribute(tableName2,temp_attr2))
		{
			cout<<"add the attribute "<<temp_attr2.name<<" successfully!!"<<endl;
		}
		else
		{
			cout<<"can not add the attribute "<<temp_attr2.name<<endl;
		}
		attrs2.clear();
		if(!rm1->getAttributes(tableName2,attrs2))
		{
			cout<<"get type of the attributes of "<<tableName2<<" successfully!!!"<<endl;
			cout<<"there are "<<attrs2.size()<<" attributes in the table"<<tableName2<<" and they are:"<<endl;
			for(i = 0; i <attrs2.size(); i++)
			{
				cout<<attrs2[i].name<<" with type "<<attrs2[i].type<<endl;
			}
		}
		else
		{
			cout<<"can not get the attributes of"<<tableName2<<endl;
		}
//
		//read the tuples in table1
		for(i = 0; i < 5; i++)
		{
			rid.pageNum = 0;
			rid.slotNum = i;
			if(!rm1->readTuple(tableName2,rid,data))
			{
				cout<<"read the tuple successfully! in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
//				data_print(data, attrs2);
			}
			else
			{
				cout<<"can't find the tuple"<<endl;
			}
		}
//		 /**************************************************************************************/
		//insert new tuples into table1 after adding attributes
		for(i = 0; i < 20; i++)
		{
					randomdata(data,attrs2,&length);
					if(!rm1->insertTuple(tableName2,data,rid))
					{
						cout<<"successfully insert the new tuples in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
					}
					else
					{
				        cout<<"insertion error!!!"<<endl;
					}
					if(!rm1->readTuple(tableName2,rid,data_output))
					{
						data_print(data_output, attrs2);
						if(streamcompare(data,data_output,length))
				    	{
							cout<<"read the tuple successfully! in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
				    	}
				    	else
				    	{
				    		cout<<"output tuple ERROR!!!"<<endl;
						}
					}
				    else
				    {
					   	cout<<"read tuple ERROR!!!"<<endl;
				    }
				}
		//read the tuples in table1
				for(i = 0; i < 50; i++)
				{
					rid.pageNum = 0;
					rid.slotNum = i;
					if(!rm1->readTuple(tableName2,rid,data))
					{
						cout<<"read the tuple successfully! in page "<<rid.pageNum<<" and slot "<<rid.slotNum<<endl;
						data_print(data, attrs2);
					}
					else
					{
						cout<<"can't find the tuple"<<endl;
					}
				}
		/*********************************************************************/
		//scan the table

		 RM_ScanIterator rmsi;
		 vector<string> projected_attributes;
		 projected_attributes.push_back(attrs2[1].name);//b int
		 projected_attributes.push_back(attrs2[2].name);//c real
		 projected_attributes.push_back(attrs2[3].name);//d varchar

		 vector<Attribute> projected_attrs;
		 projected_attrs.push_back(attrs2[1]);
		 projected_attrs.push_back(attrs2[2]);
		 projected_attrs.push_back(attrs2[3]);

		 //CompOp compOp = GT_OP;
		 string condition_attr = "c";

		 char* value = (char*)malloc(10);
		 float int_int = 0.2;
		 memcpy(value,&int_int,4);
		 //value[4] = 'y';
		 //value[5] = 'y';
		 if(!rm1->scan(tableName2, condition_attr, GT_OP, value, projected_attributes, rmsi))
		 {
			 cout<<"set the iterator successfully!"<<endl;
		 }
	     cout << "Scanned Data:" << endl;
		 while(rmsi.getNextTuple(rid, data) != RM_EOF)
		 {
			 data_print(data, projected_attrs);
			 cout<<"*********************************"<<endl;
		 }
		 rmsi.close();

		cout<<"test complete!"<<endl;

}

int main()
{
  cout << "test..." << endl;

  mytest();

  cout << "OK" << endl;
}
