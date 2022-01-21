#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include "filesys.h"



class FileSys::FindContext
{
public:
	DIR *dp=nullptr;
	std::string subDir;
	DirectoryEntry Read(std::string hostPath) const;
};
FileSys::DirectoryEntry FileSys::FindContext::Read(std::string hostPath) const
{
	DirectoryEntry ent;

	if(nullptr==dp)
	{
		ent.endOfDir=true;
		return ent;
	}
	
	struct dirent *de=readdir(dp);
	if(nullptr!=de)
	{
		if(0!=de->d_ino)
		{
			ent.endOfDir=false;

			std::string ful=hostPath+"/"+subDir+"/"+de->d_name;

			struct stat st;
			stat(ful.c_str(),&st);

			ent.fName=de->d_name;
			if(S_IFDIR==(st.st_mode&S_IFMT))
			{
				ent.attr|=ATTR_DIR;
				ent.length=0;
			}
			else
			{
				ent.attr&=~ATTR_DIR;
				ent.length=st.st_size;
			}
		}
	}
	else
	{
		ent.endOfDir=true;
	}
	return ent;
}

FileSys::FileSys()
{
	context=new FindContext;
}
FileSys::~FileSys()
{
	delete context;
	context=nullptr;
}
FileSys::DirectoryEntry FileSys::FindFirst(std::string subDir)
{
	std::string path=hostPath;
	if(""!=subDir && "/"!=subDir && "\\"!=subDir)
	{
		if(""==path || (path.back()!='/' && path.back()!='\\'))
		{
			path.push_back('/');
		}
		path+=subDir;
	}
	DirectoryEntry ent;

	if(nullptr!=context->dp)
	{
		closedir(context->dp);
	}
	context->dp=opendir(path.c_str());
	if(nullptr==context->dp)
	{
		ent.endOfDir=true;
		return ent;
	}
	context->subDir=subDir;

	ent=context->Read(hostPath);
	if(true==ent.endOfDir)
	{
		closedir(context->dp);
		context->dp=nullptr;
	}
	return ent;
}
FileSys::DirectoryEntry FileSys::FindNext(void)
{
	DirectoryEntry ent;
	ent=context->Read(hostPath);
	if(true==ent.endOfDir)
	{
		closedir(context->dp);
		context->dp=nullptr;
	}
	return ent;
}
/* static */ std::string FileSys::Getcwd(void)
{
	char buf[1024];
	if(nullptr==getcwd(buf,1023))
	{
		return "";
	}
	return buf;
}
/* static */ bool FileSys::Chdir(std::string str)
{
	if(0==chdir(str.c_str()))
	{
		return true;
	}
	return false;
}
/* static */ bool FileSys::Mkdir(std::string str)
{
	if(0==mkdir(str.c_str(),0x777))
	{
		return true;
	}
	return false;
}
