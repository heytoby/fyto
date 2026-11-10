from pathlib import Path
import sys
path=Path(sys.argv[1])/'voidImageViewer/src/viv.c'
s=path.read_text(encoding='utf-8')
old='''    if (!directory || !directory->ready || !*current->cFileName ||
        WaitForSingleObject(directory->changes, 0) != WAIT_TIMEOUT) return FALSE;
    string_get_path_part(folder, current->cFileName);
    if (string_compare(folder, directory->folder) != 0) return FALSE;'''
new='''    if (!directory || !directory->ready || !*current->cFileName) return FALSE;
    string_get_path_part(folder, current->cFileName);
    if (string_compare(folder, directory->folder) != 0) return FALSE;
    if (WaitForSingleObject(directory->changes, 0) != WAIT_TIMEOUT)
    {
        _viv_clear_loading_preload();
        _viv_clear_preload();
        _viv_clear_last();
        return FALSE;
    }'''
assert s.count(old)==1
s=s.replace(old,new)
path.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
