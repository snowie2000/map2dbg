#ifndef __PDB_H__
#define __PDB_H__

#include <mspdb.h>

#include <windows.h>
#include <map>
#include <unordered_map>
#include <ImageHlp.h>
#include <LastError.h>
#include <PEImage.h>

extern "C" {
#include <mscvpdb.h>
#include <dcvinfo.h>
}


#ifdef UNICODE
#define T_toupper	towupper
#define T_getdcwd	_wgetdcwd
#define T_strlen	wcslen
#define T_strcpy	wcscpy
#define T_strcat	wcscat
#define T_strstr	wcsstr
#define T_strtod	wcstod
#define T_strrchr	wcsrchr
#define T_unlink	_wremove
#define T_main		wmain
#define SARG		"%S"
#define T_stat		_wstat
#else
#define T_toupper	toupper
#define T_getdcwd	_getdcwd
#define T_strlen	strlen
#define T_strcpy	strcpy
#define T_strcat	strcat
#define T_strstr	strstr
#define T_strtod	strtod
#define T_strrchr	strrchr
#define T_unlink	unlink
#define T_main		main
#define SARG		"%s"
#define T_stat		stat
#endif


typedef struct {
	unsigned short seg;
	unsigned long offset;
	int type;
	std::string name;
} SYMBOL, *PSYMBOL;

typedef std::vector<PSYMBOL> SymbolList;

class PDB: public LastError
{
public:
	PDB(PLOADED_IMAGE image);
	~PDB();

	bool cleanup(bool commit);
	bool openPDB(const TCHAR* pdbname, const TCHAR* pdbref);

	bool setError(const char* msg);
	bool createModules(const char* modname);

	bool initLibraries();
	const BYTE* getLibrary(int i);
	bool initSegMap();

	enum
	{
		kCmdAdd,
		kCmdCount,
		kCmdNestedTypes,
		kCmdOffsetFirstVirtualMethod,
		kCmdHasClassTypeEnum,
		kCmdCountBaseClasses
	};

	void checkUserTypeAlloc(int size = 1000, int add = 10000);
	void checkGlobalTypeAlloc(int size, int add = 1000);
	void checkUdtSymbolAlloc(int size, int add = 10000);
	void writeUserTypeLen(codeview_type* type, int len);

	const codeview_type* getTypeData(int type);
	const codeview_type* getUserTypeData(int type);
	const codeview_type* getConvertedTypeData(int type);

	int findMemberFunctionType(codeview_symbol* lastGProcSym, int thisPtrType);
	int sizeofBasicType(int type);

	// to be used when writing new type only to avoid double translation
	int translateType(int type);


	bool initGlobalSymbols();

	bool addTypes();
	bool addSrcLines();
	bool addSrcLines14();
	bool addPublics(SymbolList sl);

	codeview_symbol* findUdtSymbol(int type);
	codeview_symbol* findUdtSymbol(const char* name);
	bool addUdtSymbol(int type, const char* name);
	
	bool markSrcLineInBitmap(int segIndex, int adr);
	bool createSrcLineBitmap();
	int  getNextSrcLine(int seg, unsigned int off);
	bool PDB::writeImage(const TCHAR* opath, PEImage& exeImage);

	mspdb::Mod* globalMod();

	// private:
	BYTE* libraries;
	bool isX64;
	PLOADED_IMAGE img;
	mspdb::PDB* pdb;
	mspdb::DBI *dbi;
	mspdb::TPI *tpi;
	mspdb::TPI *ipi;

	mspdb::Mod** modules;
	mspdb::Mod* globmod;
	int countEntries;

	OMFSignatureRSDS* rsds;
	int rsdsLen;

	OMFSegMap* segMap;
	OMFSegMapDesc* segMapDesc;
	int* segFrame2Index;

	OMFGlobalTypes* globalTypeHeader;

	unsigned char* globalTypes;
	int cbGlobalTypes;
	int allocGlobalTypes;

	unsigned char* userTypes;
	int* pointerTypes;
	int cbUserTypes;
	int allocUserTypes;

	unsigned char* globalSymbols;
	int cbGlobalSymbols;

	unsigned char* staticSymbols;
	int cbStaticSymbols;

	unsigned char* udtSymbols;
	int cbUdtSymbols;
	int allocUdtSymbols;

	unsigned char* dwarfTypes;
	int cbDwarfTypes;
	int allocDwarfTypes;

	int nextUserType;
	int nextDwarfType;
	int objectType;

	int emptyFieldListType;
	int classEnumType;
	int ifaceEnumType;
	int cppIfaceEnumType;
	int structEnumType;

	int classBaseType;
	int ifaceBaseType;
	int cppIfaceBaseType;
	int structBaseType;

	// D named types
	int typedefs[20];
	int translatedTypedefs[20];
	int cntTypedefs;

	bool addClassTypeEnum;
	bool addStringViewHelper;
	bool addObjectViewHelper;
	bool methodListToOneMethod;
	bool removeMethodLists;
	bool useGlobalMod;
	bool thisIsNotRef;
	bool v3;
	bool debug;
	const char* lastError;

	int srcLineSections;
	char** srcLineStart; // array of bitmaps per segment, indicating whether src line start is available for corresponding address

	double Dversion;
	int numsecs;
};

#endif