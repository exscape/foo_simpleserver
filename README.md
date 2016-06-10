About foo_simpleserver
======================
This is a simple IPC server that shares some information about foobar's library and status to
external programs that want to receive it.  
It is really only intended to be used with other pieces of software I've written, and so there is no
documented (or stable) API of any kind.

Build notes
-----------
You can use Visual Studio 2015 Community edition (free for personal use) to build this.  
Later versions should probably/hopefully also work. If not, the foobar2000 SDK (included)
might be outdated.

Note that the default build location is set to C:\Program Files (x86)\foobar2000\components for my convenience.  
You might want to change that -- though I do recommend using the foobar2000 location, as that makes it very easy
to compile and run (or debug).
