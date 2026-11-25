#ifndef FYTO_ASSOCIATION_PATH_H
#define FYTO_ASSOCIATION_PATH_H
/* Association state belongs to this executable, not every file named fyto.exe.
   File identity also accepts hard links and short-path aliases of this copy. */
static BOOL fyto_same_executable_file(const wchar_t *left,const wchar_t *right)
{
    HANDLE a,b;BY_HANDLE_FILE_INFORMATION ai,bi;BOOL same=FALSE;
    if(!left || !*left || !right || !*right)return FALSE;
    a=CreateFileW(left,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(a==INVALID_HANDLE_VALUE)return FALSE;
    b=CreateFileW(right,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(b!=INVALID_HANDLE_VALUE){
        if(GetFileInformationByHandle(a,&ai) && GetFileInformationByHandle(b,&bi))
            same=ai.dwVolumeSerialNumber==bi.dwVolumeSerialNumber &&
                ai.nFileIndexHigh==bi.nFileIndexHigh && ai.nFileIndexLow==bi.nFileIndexLow;
        CloseHandle(b);
    }
    CloseHandle(a);return same;
}
#endif
