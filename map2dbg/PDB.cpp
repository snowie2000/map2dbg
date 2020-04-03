
// Convert DMD CodeView debug information to PDB files
// Copyright (c) 2009-2010 by Rainer Schuetze, All Rights Reserved
//
// License for redistribution is given by the Artistic License 2.0
// see file LICENSE for further details

#include "pdb.h"
#include <symutil.h>
#include <stdio.h>
#include <direct.h>

#define REMOVE_LF_DERIVED  1  // types wrong by DMD
#define PRINT_INTERFACEVERSON 0

#define CLASSTYPEENUM_TYPE  "__ClassType"
#define CLASSTYPEENUM_NAME  "__classtype"

static const int typePrefix = 4;

PDB::PDB(PLOADED_IMAGE image)
	: img(image), pdb(0), dbi(0), tpi(0), ipi(0), libraries(0), rsds(0), rsdsLen(0), modules(0), globmod(0)
	, segMap(0), segMapDesc(0), segFrame2Index(0), globalTypeHeader(0)
	, globalTypes(0), cbGlobalTypes(0), allocGlobalTypes(0)
	, userTypes(0), cbUserTypes(0), allocUserTypes(0)
	, globalSymbols(0), cbGlobalSymbols(0), staticSymbols(0), cbStaticSymbols(0)
	, udtSymbols(0), cbUdtSymbols(0), allocUdtSymbols(0)
	, dwarfTypes(0), cbDwarfTypes(0), allocDwarfTypes(0)
	, srcLineStart(0), srcLineSections(0)
	, pointerTypes(0)
	, Dversion(2)
	, debug(false)
	, classEnumType(0), ifaceEnumType(0), cppIfaceEnumType(0), structEnumType(0)
	, classBaseType(0), ifaceBaseType(0), cppIfaceBaseType(0), structBaseType(0)
	, emptyFieldListType(0), isX64(false)	// for now, we stick to x86
{
	memset(typedefs, 0, sizeof(typedefs));
	memset(translatedTypedefs, 0, sizeof(translatedTypedefs));
	cntTypedefs = 0;
	nextUserType = 0x1000;
	nextDwarfType = 0x1000;

	addClassTypeEnum = true;
	addObjectViewHelper = true;
	addStringViewHelper = false;
	methodListToOneMethod = true;
	removeMethodLists = true;
	useGlobalMod = true;
	thisIsNotRef = true;
	v3 = true;
	countEntries = 3;
	numsecs = img->NumberOfSections;
}

PDB::~PDB()
{
	cleanup(false);
}

bool PDB::cleanup(bool commit)
{
	if (modules)
		for (int m = 0; m < countEntries; m++)
			if (modules[m])
				modules[m]->Close();
	delete[] modules;
	if (globmod)
		globmod->Close();

	if (dbi)
		dbi->SetMachineType(IMAGE_FILE_MACHINE_I386);
		//dbi->SetMachineType(img..isX64() ? IMAGE_FILE_MACHINE_AMD64 : IMAGE_FILE_MACHINE_I386);

	if (dbi)
		dbi->Close();
	if (tpi)
		tpi->Close();
	if (ipi)
		ipi->Close();
	if (pdb)
		pdb->Commit();
	if (pdb)
		pdb->Close();

	if (rsds)
		delete[](char*) rsds;
	if (globalTypes)
		free(globalTypes);
	if (userTypes)
		free(userTypes);
	if (udtSymbols)
		free(udtSymbols);
	if (dwarfTypes)
		free(dwarfTypes);
	delete[] pointerTypes;

	for (int i = 0; i < srcLineSections; i++)
		delete[] srcLineStart[i];
	delete[] srcLineStart;
	srcLineStart = 0;
	srcLineSections = 0;

	delete[] segFrame2Index;
	segFrame2Index = 0;

	globalTypes = 0;
	cbGlobalTypes = 0;
	allocGlobalTypes = 0;
	userTypes = 0;
	cbUserTypes = 0;
	allocUserTypes = 0;
	globalSymbols = 0;
	cbGlobalSymbols = 0;
	staticSymbols = 0;
	cbStaticSymbols = 0;
	udtSymbols = 0;
	cbUdtSymbols = 0;
	allocUdtSymbols = 0;
	cbDwarfTypes = 0;
	allocDwarfTypes = 0;
	modules = 0;
	globmod = 0;
	countEntries = 0;
	dbi = 0;
	pdb = 0;
	rsds = 0;
	segMap = 0;
	segMapDesc = 0;
	globalTypeHeader = 0;
	pointerTypes = 0;
	memset(typedefs, 0, sizeof(typedefs));
	memset(translatedTypedefs, 0, sizeof(translatedTypedefs));
	cntTypedefs = 0;

	return true;
}

bool PDB::openPDB(const TCHAR* pdbname, const TCHAR* pdbref)
{
#ifdef UNICODE
	const wchar_t* pdbnameW = pdbname;
	char pdbnameA[260]; // = L"c:\\tmp\\aa\\ddoc4.pdb";
	WideCharToMultiByte(CP_UTF8, 0, pdbref ? pdbref : pdbname, -1, pdbnameA, 260, 0, 0);
	//  wcstombs (pdbnameA, pdbname, 260);
#else
	const char* pdbnameA = pdbref ? pdbref : pdbname;
	wchar_t pdbnameW[260]; // = L"c:\\tmp\\aa\\ddoc4.pdb";
	mbstowcs(pdbnameW, pdbname, 260);
#endif

	if (!initMsPdb())
		return setError("cannot load PDB helper DLL");
	if (debug)
	{
		extern HMODULE modMsPdb;
		char modpath[260];
		GetModuleFileNameA(modMsPdb, modpath, 260);
		printf("Loaded PDB helper DLL: %s\n", modpath);
	}
	pdb = CreatePDB(pdbnameW);
	if (!pdb)
		return setError("cannot create PDB file");

#if PRINT_INTERFACEVERSON
	printf("PDB::QueryInterfaceVersion() = %d\n", pdb->QueryInterfaceVersion());
	printf("PDB::QueryImplementationVersion() = %d\n", pdb->QueryImplementationVersion());
	printf("PDB::QueryPdbImplementationVersion() = %d\n", pdb->QueryPdbImplementationVersion());
#endif

	rsdsLen = 24 + strlen(pdbnameA) + 1; // sizeof(OMFSignatureRSDS) without name
	rsds = (OMFSignatureRSDS *) new char[rsdsLen];
	memcpy(rsds->Signature, "RSDS", 4);
	pdb->QuerySignature2(&rsds->guid);
	rsds->age = pdb->QueryAge();
	strcpy(rsds->name, pdbnameA);

	int rc = pdb->CreateDBI("", &dbi);
	if (rc <= 0 || !dbi)
		return setError("cannot create DBI");

#if PRINT_INTERFACEVERSON
	printf("DBI::QueryInterfaceVersion() = %d\n", dbi->QueryInterfaceVersion());
	printf("DBI::QueryImplementationVersion() = %d\n", dbi->QueryImplementationVersion());
#endif

	rc = pdb->OpenTpi("", &tpi);
	if (rc <= 0 || !tpi)
		return setError("cannot create TPI");

#if 0
	if (mspdb::vsVersion >= 14)
	{
		rc = pdb->OpenIpi("", &ipi);
		if (rc <= 0 || !ipi)
			return setError("cannot create IPI");
	}
#endif

#if PRINT_INTERFACEVERSON
	printf("TPI::QueryInterfaceVersion() = %d\n", tpi->QueryInterfaceVersion());
	printf("TPI::QueryImplementationVersion() = %d\n", tpi->QueryImplementationVersion());
#endif

	// only add helper for VS2012 or earlier, that default to the old debug engine
	addClassTypeEnum = mspdb::vsVersion < 12;
	addStringViewHelper = mspdb::vsVersion < 12;
	addObjectViewHelper = mspdb::vsVersion < 12;
	return true;
}

bool PDB::setError(const char* msg)
{
	char pdbmsg[256];
	if (pdb)
		pdb->QueryLastError(pdbmsg);
	return LastError::setError(msg);
}

bool PDB::createModules(const char* modname)
{
	// assumes libraries and segMap initialized
	modules = new mspdb::Mod*[countEntries];
	memset(modules, 0, countEntries * sizeof(*modules));

	OMFModule module;
	module.ovlNumber = 0;
	module.iLib = 0;
	module.cSeg = (unsigned short)numsecs;
	module.Style[0] = 'C';
	module.Style[1] = 'V';

	const BYTE* plib = getLibrary(module.iLib);
	const char* lib = (!plib || !*plib ? modname : p2c(plib, 1));

	mspdb::Mod* mod;
	mod = globalMod();
	if (!mod)
		return false;

#if PRINT_INTERFACEVERSON
	static bool once;
	if (!once)
	{
		printf("Mod::QueryInterfaceVersion() = %d\n", mod->QueryInterfaceVersion());
		printf("Mod::QueryImplementationVersion() = %d\n", mod->QueryImplementationVersion());
		once = true;
	}
#endif

	for (int s = 0; s < module.cSeg; s++)
	{
		int segIndex = (unsigned short)(s + 1);
		int segFlags = 0;
		if (segMap && segIndex < segMap->cSeg)
			segFlags = 0;
		segFlags = 0x60101020; // 0x40401040, 0x60500020; // TODO
		int rc = mod->AddSecContrib(segIndex, 0, img->Sections[s].Misc.VirtualSize, segFlags);
		if (rc <= 0)
			return setError("cannot add section contribution to module");
	}

	return true;
}

mspdb::Mod* PDB::globalMod()
{
	if (!globmod)
	{
		int rc = dbi->OpenMod("__Globals", "__Globals", &globmod);
		if (rc <= 0 || !globmod)
			setError("cannot create global module");
	}
	return globmod;
}

bool PDB::initLibraries()
{
	libraries = 0;
	// our dbg don't have a library section.
	return true;
}

const BYTE* PDB::getLibrary(int i)
{
	if (!libraries)
		return 0;
	const BYTE* p = libraries;
	for (int j = 0; j < i; j++)
		p += *p + 1;
	return p;
}

bool PDB::initSegMap()
{
	int numsecs = img->NumberOfSections;
	OMFSegMap omfSegMap = { (unsigned short)numsecs,(unsigned short)numsecs };
	segMap = &omfSegMap;
	int maxframe = -1;
	for (int s = 1; s <= segMap->cSeg; s++)
	{
		int rc = dbi->AddSec((unsigned short)s, 0, 0, img->Sections[s - 1].Misc.VirtualSize);
		if (rc <= 0)
			return setError("cannot add section");
		if (s > maxframe)
			maxframe = s;
	}

	segFrame2Index = new int[maxframe + 1];
	memset(segFrame2Index, -1, (maxframe + 1) * sizeof(*segFrame2Index));
	for (int s = 0; s < segMap->cSeg; s++)
		segFrame2Index[s+1] = s;

	return true;
}


static int copy_p2dsym(unsigned char* dp, int& dpos, const unsigned char* p, int& pos, int maxdlen)
{
	const BYTE* q = p + pos;
	int plen = pstrlen(q);
	int len = min(plen, maxdlen - dpos);
	memcpy(dp + dpos, q, len);
	dp[dpos + len] = 0;
	dpos += len + 1;
	pos = q - p + plen;
	return len;
}

void PDB::checkUserTypeAlloc(int size, int add)
{
	if (cbUserTypes + size >= allocUserTypes)
	{
		allocUserTypes = allocUserTypes * 4 / 3 + size + add;
		userTypes = (BYTE*)realloc(userTypes, allocUserTypes);
		if (!userTypes)
			setError("out of memory");
	}
}

void PDB::writeUserTypeLen(codeview_type* type, int len)
{
	unsigned char* p = (unsigned char*)type;
	for (; len & 3; len++)
		p[len] = 0xf4 - (len & 3);

	type->generic.len = len - 2;
	cbUserTypes += len;
}

void PDB::checkGlobalTypeAlloc(int size, int add)
{
	if (cbGlobalTypes + size > allocGlobalTypes)
	{
		allocGlobalTypes = allocGlobalTypes * 4 / 3 + size + add;
		globalTypes = (unsigned char*)realloc(globalTypes, allocGlobalTypes);
		if (!globalTypes)
			setError("out of memory");
	}
}

const codeview_type* PDB::getTypeData(int type)
{
	if (!globalTypeHeader)
		return 0;
	if (type < 0x1000 || type >= (int)(0x1000 + globalTypeHeader->cTypes + nextUserType))
		return 0;
	if (type >= (int)(0x1000 + globalTypeHeader->cTypes))
		return getUserTypeData(type);

	DWORD* offset = (DWORD*)(globalTypeHeader + 1);
	BYTE* typeData = (BYTE*)(offset + globalTypeHeader->cTypes);

	return (codeview_type*)(typeData + offset[type - 0x1000]);
}

const codeview_type* PDB::getUserTypeData(int type)
{
	type -= 0x1000 + globalTypeHeader->cTypes;
	if (type < 0 || type >= nextUserType - 0x1000)
		return 0;

	int pos = 0;
	while (type > 0 && pos < cbUserTypes)
	{
		const codeview_type* ptype = (codeview_type*)(userTypes + pos);
		int len = ptype->generic.len + 2;
		pos += len;
		type--;
	}
	return (codeview_type*)(userTypes + pos);
}

const codeview_type* PDB::getConvertedTypeData(int type)
{
	type -= 0x1000;
	if (type < 0 || type >= nextUserType - 0x1000)
		return 0;

	int pos = typePrefix;
	while (type > 0 && pos < cbGlobalTypes)
	{
		const codeview_type* ptype = (codeview_type*)(globalTypes + pos);
		int len = ptype->generic.len + 2;
		pos += len;
		type--;
	}
	return (codeview_type*)(globalTypes + pos);
}

int PDB::findMemberFunctionType(codeview_symbol* lastGProcSym, int thisPtrType)
{
	const codeview_type* proctype = getTypeData(lastGProcSym->proc_v2.proctype);
	if (!proctype || proctype->generic.id != LF_PROCEDURE_V1)
		return lastGProcSym->proc_v2.proctype;

	const codeview_type* thisPtrData = getTypeData(thisPtrType);
	if (!thisPtrData || thisPtrData->generic.id != LF_POINTER_V1)
		return lastGProcSym->proc_v2.proctype;

	int thistype = thisPtrData->pointer_v1.datatype;

	// search method with same arguments and return type
	DWORD* offset = (DWORD*)(globalTypeHeader + 1);
	BYTE* typeData = (BYTE*)(offset + globalTypeHeader->cTypes);
	for (unsigned int t = 0; t < globalTypeHeader->cTypes; t++)
	{
		// remember: mfunction_v1.class_type falsely is pointer, not class type
		const codeview_type* type = (const codeview_type*)(typeData + offset[t]);
		if (type->generic.id == LF_MFUNCTION_V1 && type->mfunction_v1.this_type == thisPtrType)
		{
			if (type->mfunction_v1.arglist == proctype->procedure_v1.arglist &&
				type->mfunction_v1.call == proctype->procedure_v1.call &&
				type->mfunction_v1.rvtype == proctype->procedure_v1.rvtype)
			{
				return t + 0x1000;
			}
		}
	}
	return lastGProcSym->proc_v2.proctype;
}

int PDB::sizeofBasicType(int type)
{
	int size = type & 7;
	int typ = (type & 0xf0) >> 4;
	int mode = (type & 0x700) >> 8;

	switch (mode)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 5: // pointer variations
		return 4;
	case 6: // 64-bit pointer
		return 8;
	case 7: // reserved
		return 4;
	case 0: // not pointer
		switch (typ)
		{
		case 0: // special, cannot determine
			return 4;
		case 1:
		case 2: // integral types
			switch (size)
			{
			case 0: return 1;
			case 1: return 2;
			case 2: return 4;
			case 3: return 8;
				// other reserved
			}
			return 4;
		case 3: // boolean
			return 1;
		case 4:
		case 5: // real and complex
			switch (size)
			{
			case 0: return 4;
			case 1: return 8;
			case 2: return 10;
			case 3: return 16;
			case 4: return 6;
				// other reserved
			}
			return 4;
		case 6: // special2 (bit or pascal char)
			return 1;
		case 7: // real int
			switch (size)
			{
			case 0: return 1; // char
			case 1: return 4; // wide char
			case 2: return 2;
			case 3: return 2;
			case 4: return 4;
			case 5: return 4;
			case 6: return 8;
			case 7: return 8;
			}
		}
	}
	return 4;
}

// to be used when writing new type only to avoid double translation
int PDB::translateType(int type)
{
	if (type < 0x1000)
	{
		for (int i = 0; i < cntTypedefs; i++)
			if (type == typedefs[i])
				return translatedTypedefs[i];
		return type;
	}

	const codeview_type* cvtype = getTypeData(type);
	if (!cvtype)
		return type;

	if (cvtype->generic.id != LF_OEM_V1)
		return type;

	codeview_oem_type* oem = (codeview_oem_type*)(&cvtype->generic + 1);
	if (oem->generic.oemid == 0x42 && oem->generic.id == 3)
	{
		if (oem->d_delegate.this_type == 0x403 && oem->d_delegate.func_type == 0x74)
			return translateType(0x13); // int64
	}
	if (oem->generic.oemid == 0x42 && oem->generic.id == 1 && Dversion == 0)
	{
		// C does not have D types, so this must be unsigned long
		if (oem->d_dyn_array.index_type == 0x12 && oem->d_dyn_array.elem_type == 0x74)
			return translateType(0x23); // unsigned int64
	}

	return type;
}


bool PDB::addTypes()
{
	if (!globalTypes)
		return true;

	if (useGlobalMod)
	{
		int rc = globalMod()->AddTypes(globalTypes, cbGlobalTypes);
		if (rc <= 0)
			return setError("cannot add type info to module");
		return true;
	}

	return true;
}

bool PDB::markSrcLineInBitmap(int segIndex, int adr)
{
	if (segIndex < 0 || segIndex >= segMap->cSeg)
		return setError("invalid segment info in line number info");

	int off = adr - segMapDesc[segIndex].offset;
	if (off < 0 || off >= (int)segMapDesc[segIndex].cbSeg)
		return setError("invalid segment offset in line number info");

	srcLineStart[segIndex][off] = true;
	return true;
}

bool PDB::createSrcLineBitmap()
{
	if (srcLineStart)
		return true;
	if (!segMap || !segMapDesc || !segFrame2Index)
		return false;

	return true;
}

int PDB::getNextSrcLine(int seg, unsigned int off)
{
	if (!createSrcLineBitmap())
		return -1;

	int s = segFrame2Index[seg];
	if (s < 0)
		return -1;

	off -= segMapDesc[s].offset;
	if (off < 0 || off >= segMapDesc[s].cbSeg || off > LONG_MAX)
		return 0;

	for (off++; off < segMapDesc[s].cbSeg; off++)
		if (srcLineStart[s][off])
			break;

	return off + segMapDesc[s].offset;
}

bool PDB::addSrcLines()
{
	if (mspdb::vsVersion >= 14)
		return addSrcLines14();

	return true;
}

////////////////////////////////////////
template<typename T>
void append(std::vector<char>& v, const T& x)
{
	size_t sz = v.size();
	v.resize(sz + sizeof(T));
	memcpy(v.data() + sz, &x, sizeof(T));
}

void append(std::vector<char>& v, const void* data, size_t len)
{
	size_t sz = v.size();
	v.resize(sz + len);
	memcpy(v.data() + sz, data, len);
}

void align(std::vector<char>& v, int algn)
{
	while (v.size() & (algn - 1))
		v.push_back(0);
}

int addfile(std::vector<char>& f3, std::vector<char>& f4, const char* s)
{
	size_t slen = strlen(s);
	const char* p = f3.data();
	size_t plen = strlen(p);
	int fileno = -1; // don't count initial 0
	while (plen != slen || strncmp(p, s, slen) != 0)
	{
		p += plen + 1;
		fileno++;
		if (p - f3.data() >= (ptrdiff_t)f3.size())
		{
			size_t pos = f3.size();
			append(f3, s, slen + 1);

			append(f4, (int)pos);
			append(f4, (int)0); // checksum
			return fileno * 8;
		}
		plen = strlen(p);
	}
	return fileno * 8; // offset in source list
}

////////////////////////////////////////
bool PDB::addSrcLines14()
{
	if (!useGlobalMod)
		return setError("unexpected call of addSrcLines14()");

	return true;
}

bool PDB::addPublics(SymbolList sl)
{
	OMFDirEntry entry;
	entry.SubSection = sstGlobalPub;
	entry.iMod = 0xFFFF;

	mspdb::Mod* mod = 0;
	if (entry.iMod < countEntries)
		mod = useGlobalMod ? globalMod() : modules[entry.iMod];

	OMFSymHash header;
	header.cbSymbol = sl.size(); //gpoSym - sizeof(OMFSymHash);
	header.symhash = 0; // No symbol or address hash tables...
	header.addrhash = 0;
	header.cbHSym = 0;
	header.cbHAddr = 0;
	PSYMBOL* symlist = sl.data();
	for (unsigned int i = 0; i < header.cbSymbol; i++)
	{
		//length = sym->generic.len + 2;
		//if (!sym->generic.id || length < 4)
		//	break;
		SYMBOL& sym = *symlist[i];
		int rc;
		char symname[kMaxNameLen];
		//dsym2c((BYTE*)sym->data_v1.p_name.name, sym->data_v1.p_name.namelen, symname, sizeof(symname));
		int type = translateType(sym.type);
		if (mod)
			rc = mod->AddPublic2(sym.name.c_str(), sym.seg, sym.offset, type);
		else
			rc = dbi->AddPublic2(sym.name.c_str(), sym.seg, sym.offset, type);
		if (rc <= 0)
			return setError("cannot add public");
	}

	return true;
}

bool PDB::initGlobalSymbols()
{
	return true;
}

bool isUDTid(int id)
{
	return id == S_UDT_V1 || id == S_UDT_V2 || id == S_UDT_V3;
}

codeview_symbol* PDB::findUdtSymbol(int type)
{
	type = translateType(type);
	for (int p = 0; p < cbGlobalSymbols; )
	{
		codeview_symbol* sym = (codeview_symbol*)(globalSymbols + p);
		if (isUDTid(sym->generic.id) && sym->udt_v1.type == type)
			return sym;
		p += sym->generic.len + 2;
	}
	for (int p = 0; p < cbStaticSymbols; )
	{
		codeview_symbol* sym = (codeview_symbol*)(staticSymbols + p);
		if (isUDTid(sym->generic.id) && sym->udt_v1.type == type)
			return sym;
		p += sym->generic.len + 2;
	}
	for (int p = 0; p < cbUdtSymbols; )
	{
		codeview_symbol* sym = (codeview_symbol*)(udtSymbols + p);
		if (isUDTid(sym->generic.id) && sym->udt_v1.type == type)
			return sym;
		p += sym->generic.len + 2;
	}
	return 0;
}

bool isUDT(codeview_symbol* sym, const char* name)
{
	return (sym->generic.id == S_UDT_V1 && p2ccmp(sym->udt_v1.p_name, name) ||
		sym->generic.id == S_UDT_V2 && p2ccmp(sym->udt_v2.p_name, name) ||
		sym->generic.id == S_UDT_V3 && strcmp(sym->udt_v3.name, name) == 0);
}

codeview_symbol* PDB::findUdtSymbol(const char* name)
{
	for (int p = 0; p < cbGlobalSymbols; )
	{
		codeview_symbol* sym = (codeview_symbol*)(globalSymbols + p);
		if (isUDT(sym, name))
			return sym;
		p += sym->generic.len + 2;
	}
	for (int p = 0; p < cbStaticSymbols; )
	{
		codeview_symbol* sym = (codeview_symbol*)(staticSymbols + p);
		if (isUDT(sym, name))
			return sym;
		p += sym->generic.len + 2;
	}
	for (int p = 0; p < cbUdtSymbols; )
	{
		codeview_symbol* sym = (codeview_symbol*)(udtSymbols + p);
		if (isUDT(sym, name))
			return sym;
		p += sym->generic.len + 2;
	}
	return 0;
}

void PDB::checkUdtSymbolAlloc(int size, int add)
{
	if (cbUdtSymbols + size > allocUdtSymbols)
	{
		allocUdtSymbols = allocUdtSymbols * 4 / 3 + size + add;
		udtSymbols = (BYTE*)realloc(udtSymbols, allocUdtSymbols);
		if (!udtSymbols)
			setError("out of memory");
	}
}

bool PDB::addUdtSymbol(int type, const char* name)
{
	checkUdtSymbolAlloc(100 + kMaxNameLen);

	// no need to convert to udt_v2/udt_v3, the debugger is fine with it.
	codeview_symbol* sym = (codeview_symbol*)(udtSymbols + cbUdtSymbols);
	sym->udt_v2.id = v3 ? S_UDT_V3 : S_UDT_V2;
	sym->udt_v2.type = translateType(type);
	int len = cstrcpy_v(v3, (BYTE*)&sym->udt_v2.p_name, name ? name : ""); // allow anonymous typedefs
	sym->udt_v2.len = sizeof(sym->udt_v2) - sizeof(sym->udt_v2.p_name) + len - 2;
	cbUdtSymbols += sym->udt_v2.len + 2;

	return true;
}


bool PDB::writeImage(const TCHAR* opath, PEImage& exeImage)
{
	if (!exeImage.replaceDebugSection(rsds, rsdsLen, true))
		return setError(exeImage.getLastError());

	if (!exeImage.save(opath))
		return setError(exeImage.getLastError());

	return true;
}
