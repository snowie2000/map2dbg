/***    cvexefmt.h - format of CodeView information in exe
 *
 *      Structures, constants, etc. for reading CodeView information
 *      from the executable.
 *
 */

//  The following structures and constants describe the format of the
//  CodeView Debug OMF for that will be accepted by CodeView 4.0 and
//  later.  These are executables with signatures of NB05, NB06 and NB08.
//  There is some confusion about the signatures NB03 and NB04 so none
//  of the utilites will accept executables with these signatures.  NB07 is
//  the signature for QCWIN 1.0 packed executables.

//  All of the structures described below must start on a long word boundary
//  to maintain natural alignment.  Pad space can be inserted during the
//  write operation and the addresses adjusted without affecting the contents
//  of the structures.

#ifndef _CV_INFO_INCLUDED
#include "cvinfo.h"
#endif

#ifndef    FAR
#if _M_IX86 >= 300
#define    FAR
#else
#define FAR far
#endif
#endif

#pragma warning( push )
#pragma warning( disable : 4200 )

//  Type of subsection entry.

#define sstModule           0x120
#define sstTypes            0x121
#define sstPublic           0x122
#define sstPublicSym        0x123   // publics as symbol (waiting for link)
#define sstSymbols          0x124
#define sstAlignSym         0x125
#define sstSrcLnSeg         0x126   // because link doesn't emit SrcModule
#define sstSrcModule        0x127
#define sstLibraries        0x128
#define sstGlobalSym        0x129
#define sstGlobalPub        0x12a
#define sstGlobalTypes      0x12b
#define sstMPC              0x12c
#define sstSegMap           0x12d
#define sstSegName          0x12e
#define sstPreComp          0x12f   // precompiled types
#define sstPreCompMap       0x130   // map precompiled types in global types
#define sstOffsetMap16      0x131
#define sstOffsetMap32      0x132
#define sstFileIndex        0x133   // Index of file names
#define sstStaticSym        0x134


//  The following structures and constants describe the format of the
//  CodeView Debug OMF for linkers that emit executables with the NB02
//  signature.  Current utilities with the exception of cvpack and cvdump
//  will not accept or emit executables with the NB02 signature.  Cvdump
//  will dump an unpacked executable with the NB02 signature.  Cvpack will
//  read an executable with the NB02 signature but the packed executable
//  will be written with the table format, contents and signature of NB08.


//  subsection type constants

#define SSTMODULE       0x101    // Basic info. about object module
#define SSTPUBLIC       0x102    // Public symbols
#define SSTTYPES        0x103    // Type information
#define SSTSYMBOLS      0x104    // Symbol Data
#define SSTSRCLINES     0x105    // Source line information
#define SSTLIBRARIES    0x106    // Names of all library files used
#define SSTIMPORTS      0x107    // Symbols for DLL fixups
#define SSTCOMPACTED    0x108    // Compacted types section
#define SSTSRCLNSEG     0x109    // Same as source lines, contains segment


typedef struct DirEntry{
    unsigned short  SubSectionType;
    unsigned short  ModuleIndex;
    long            lfoStart;
    unsigned short  Size;
} DirEntry;


//  information decribing each segment in a module

typedef struct oldnsg {
    unsigned short  Seg;         // segment index
    unsigned short  Off;         // offset of code in segment
    unsigned short  cbSeg;       // number of bytes in segment
} oldnsg;


//  old subsection module information

typedef struct oldsmd {
    oldnsg          SegInfo;     // describes first segment in module
    unsigned short  ovlNbr;      // overlay number
    unsigned short  iLib;
    unsigned char   cSeg;        // Number of segments in module
    char            reserved;
    unsigned char   cbName[1];   // length prefixed name of module
    oldnsg          arnsg[];     // cSeg-1 structures exist for alloc text or comdat code
} oldsmd;

typedef struct{
    unsigned short  Seg;
    unsigned long   Off;
    unsigned long   cbSeg;
} oldnsg32;

typedef struct {
    oldnsg32        SegInfo;     // describes first segment in module
    unsigned short  ovlNbr;      // overlay number
    unsigned short  iLib;
    unsigned char   cSeg;        // Number of segments in module
    char            reserved;
    unsigned char   cbName[1];   // length prefixed name of module
    oldnsg32        arnsg[];     // cSeg-1 structures exist for alloc text or comdat code
} oldsmd32;

#pragma warning( pop )