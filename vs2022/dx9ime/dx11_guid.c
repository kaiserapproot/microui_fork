#include <initguid.h>
#include <d3d11.h>

// Define the GUID for ID3D11Texture2D
#ifdef __cplusplus
extern "C" {
#endif

// This GUID definition matches the one from d3d11.h
// {6f15aaf2-d208-4e89-9ab4-489535d34f9c}
EXTERN_C const GUID IID_ID3D11Texture2D = 
    {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};

#ifdef __cplusplus
}
#endif