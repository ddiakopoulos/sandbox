#if (_MSC_VER)
    #pragma warning (push)
    #pragma warning (disable: 181 111 4267 4996 4244 4701 4702 4133 4100 4127 4206 4312 4505 4365 4005 4013 4334)
#endif

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wconversion"
    #pragma clang diagnostic ignored "-Wshadow"
    #pragma clang diagnostic ignored "-Wdeprecated-register"
#endif

#include "efsw/Debug.cpp"
#include "efsw/DirectorySnapshot.cpp"
#include "efsw/DirectorySnapshotDiff.cpp"
#include "efsw/DirWatcherGeneric.cpp"
#include "efsw/FileInfo.cpp"
#include "efsw/FileSystem.cpp"
#include "efsw/FileWatcher.cpp"
#include "efsw/FileWatcherGeneric.cpp"
#include "efsw/FileWatcherImpl.cpp"
#include "efsw/Log.cpp"
#include "efsw/Mutex.cpp"
#include "efsw/String.cpp"
#include "efsw/System.cpp"
#include "efsw/Thread.cpp"
#include "efsw/Watcher.cpp"
#include "efsw/WatcherGeneric.cpp"

#if (_MSC_VER)
	#include "efsw/FileWatcherWin32.cpp"
	#include "efsw/WatcherWin32.cpp"
	#include "efsw/platform/win/FileSystemImpl.cpp"
	#include "efsw/platform/win/MutexImpl.cpp"
	#include "efsw/platform/win/SystemImpl.cpp"
	#include "efsw/platform/win/ThreadImpl.cpp"
#endif

#ifdef __clang__
	#include "efsw/FileWatcherKqueue.cpp"
	#include "efsw/WatcherKqueue.cpp"
	#include "efsw/platform/posix/FileSystemImpl.cpp"
	#include "efsw/platform/posix/MutexImpl.cpp"
	#include "efsw/platform/posix/SystemImpl.cpp"
	#include "efsw/platform/posix/ThreadImpl.cpp"
#endif

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#if (_MSC_VER)
    #pragma warning (pop)
#endif