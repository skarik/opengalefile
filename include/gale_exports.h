#ifndef GALEFILE_2_EXPORTS_H_
#define GALEFILE_2_EXPORTS_H_

#ifndef GAL2DEF
#ifdef GAL2_USE_AS_LIBRARY
#define GAL2DEF __declspec(dllexport)
#else
#define GAL2DEF 
#endif
#endif//GAL2DEF

#endif//GALEFILE_2_EXPORTS_H_