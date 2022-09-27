#include <iostream>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <stack>
#include <pwd.h>
#include <grp.h>
#define clear printf("\033[H\033[J"); //To clear the entire screen
#define UP 65
#define LEFT  68
#define RIGHT  67
#define DOWN  66
#define BACK  127
#define ESC 27
#define BR 91
#define ENTER 10
#define clearline(x) printf("\033[%d;%dH\003[K",x,1); //To clear line number passed to it
#define clearcurline printf("\033[2K"); //To clear current line
using namespace std;

//*************************Global Variable section*****************************//

struct termios def_term; //Stores the default terminal settings
struct termios my_term; //Stores the current terminal settings
int mode=0,rows=0,cols=0; //mode-> Command/Normal mode, rows,cols->Current window size
int curx=1,cury=1; //Tracks the x and y position of the cursor
int rsflag=0; //Flag to check if the terminal is resized
int cont_sz; //cont->sz=Number of entries in the current directory
int cont_off=0; //To track the point from which the files to be displayed in case of overflow
char app_root[1000]; //Stores the directory at which the program is launched
string cur_dr,usr_home; //cur_dr=current working dir, usr_home=user home dir
string cmd_cur_dir; //Current working dir in command mode
struct winsize term_sz; //Struct to retrieve terminal size
//structure to store file metadata
struct file_meta{
	string file_name;
	long file_size;
	string user;
	string group;
	string perm;
	string mod_time;
};
vector <struct file_meta> cur_files; //This will store the metadata of files present in the current directory
stack <string> trace_back;  //stack to store previously visited directories of current one
stack <string> trace_front; //stack to store directories visited from current one

//****************************************************************************//


//*******************Function Declarations Start Here*************************//

unsigned octal(unsigned);
char filetype(int);
void get_content(const char *);
bool compare(struct file_meta f1,struct file_meta f2);
void display_content(int ,int);
static void term_sz_ch(int);
void terminal_util();
void on_start();
void normal_mode();
void godown();
void goup();
void goleft();
void goright();
void gotop();
void goend();
void goline(int);
void opendirect(string);
void openfd();
void restore_to_default();
string get_path(string);
vector <string> split(string);
int copy_file(string,string);
int copy_dir(string,string);
void copy(string);
int move_file(string,string);
int move_dir(string,string);
void move(string);
void rename_file(string);
int create_file(string);
int create_dir(string);
int del_file(string);
int del_dir(string);
void goto_loc(string);
int search(string,string);
void command_mode();

//**********************Function Declarations End Here*************************//


//This function converts given number to octal and returns it
//This is used to extract file/directory permissions from st_mode field
unsigned octal(unsigned num)
{
	unsigned x=num,mul=1,res=0;
	while(x>0)
	{
		int cur=x%8;
		res=cur*mul+res;
		mul*=10;
		x=x/8;
	}
	return res;
}

//This function will return the character corresponding to the given file type
char filetype(int x)
{
	if(x==DT_UNKNOWN)
		return '?';
	if(x==DT_REG)
		return '-';
	if(x==DT_DIR)
		return 'd';
    if(x==DT_CHR)
        return 'c';
    if(x==DT_BLK)
        return 'b';
    if(x==DT_SOCK)
        return 's';  
    if(x==DT_FIFO)
        return 'p';
    if(x==DT_LNK)
        return 'l'; 
	return ' ';
}

//This function will open the directory passed to it, reads its content and stores it in a vector
void get_content(const char * path)
{
	//Stored all the possible file permissions
	string p[8]={"---","--x","-w-","-wx","r--","r-x","rw-","rwx"};
	DIR *dptr;
	struct dirent *cur=(struct dirent *)malloc(sizeof(struct dirent));
    struct stat *file=(struct stat *)malloc(sizeof(struct stat));
	struct tm *mod=(struct tm*) malloc(sizeof(struct tm));
    unsigned perms;
    int d1=0,d2=0,d3=0;
    char c;
	//Opened the directory
	dptr=opendir(path);
	if(dptr!=NULL)
	{
		vector <struct file_meta> temp_files;
		//Reading the content of the directory
		while((cur=readdir(dptr))!=NULL)
		{	
			struct file_meta file1;
			file1.file_name=cur->d_name;
			//Reading the metadata of the directory entry
			if(stat(cur->d_name,file)==0)
			{
				//extracting the permission bits from st_mode
				perms=octal(file->st_mode)%1000;
				d1=perms/100;d2=(perms%100)/10;d3=perms%10;
				//finding the type of a file
				c=filetype(cur->d_type);
				file1.perm=c+p[d1]+p[d2]+p[d3];
				//Converting seconds to date time format 
				time_t s=file->st_mtime;
				localtime_r(&s,mod);
				char mod_tm[100];
				strftime(mod_tm, sizeof(mod_tm), "%d-%m-%Y %H:%M:%S", mod);
				strftime(mod_tm, sizeof(mod_tm), "%a %b %d %Y %H:%M:%S", mod);
				file1.mod_time=mod_tm;
				//storing file size
				file1.file_size=file->st_size;
				//storing user and group names
				struct passwd *pwd=(struct passwd *)malloc(sizeof(struct passwd));
				pwd=getpwuid(file->st_uid);
				if(!pwd)
					file1.user=to_string(file->st_uid);
				else
					file1.user=pwd->pw_name;
				struct group *g=(struct group *)malloc(sizeof(struct group));
				g=getgrgid(file->st_gid);
				if(!g)
					file1.user=to_string(file->st_gid);
				else
					file1.group=g->gr_name;
				temp_files.push_back(file1);
			}
		}
		//Storing the file metadata in the global vector
		cur_files=temp_files;
		//Making the display offset 0 in order to diaplay from the first index of vector
		cont_off=0;
		cont_sz=cur_files.size();
		//Sorting the entries by name in lexographical order
		sort(cur_files.begin(),cur_files.end(),compare);
	}
	//Closing the directory
	closedir(dptr);
    return;
}

//Compare function, which is passed to custom sort the file_meta structures
bool compare(struct file_meta f1,struct file_meta f2)
{
	if(f1.file_name.compare(f2.file_name)<0)
		return true;
	else
		return false;
}

/*This function displays the current directory entries based from offset until the screen is filled 
or vector size is exceeded*/
void display_content(int rows,int offset)
{
	//Clear the entire screen
	clear;	
	//Position the cursor at the top left corner
	printf("\033[1;1H"); 
    int sz=cur_files.size();
	int val;//=(rows+offset<(sz-offset))?rows+offset:sz-offset;
	//Get the minimum of window size and offset+window size  
	if(rows>=cont_sz)
		val=cont_sz;
	else
		val=offset+rows;

	//Prints the content of file_meta data vector from offset until given val
	for(int i=offset;i<val;i++)
	{
		struct file_meta f=cur_files[i];
		string sz=to_string(f.file_size)+" B";
		printf("%-10s\t%-10s\t%-10s\t%-10s\t%-20s\t%s\n",
		sz.c_str(),f.user.c_str(),f.group.c_str(),f.perm.c_str(),f.mod_time.c_str(),f.file_name.c_str());
	}
	// If mode is normal mode, then print Normal mode in the last line
	if(mode==0)
	{
		printf("\033[%d;%dH\033[K",rows+1,1);	
		printf("*****Normal Mode*****");
		printf("\033[%d;%dH",1,1);	
		/*if(rsflag==1)
		{
			printf("\033[%d;%dH\033[K",rows+1,1);	
			printf("*****Normal Mode*****");
			printf("\033[%d;%dH",1,1);	
		}*/
	}
	//If the mode is command mode, then print Command mode in second line form the last and clear the last line
	else
	{
		goline(rows);
		printf("***Command Mode***");
		goend();
	}
}

//This function is used to handle terminal resize signal
static void term_sz_ch(int num)
{
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_sz);
	rows=term_sz.ws_row;
	cols=term_sz.ws_col;
	rows--;
	rsflag=1;
	display_content(rows,cont_off);
}

//This function retrieves the window size at the start of execution 
//and also sets the function to be invoked in case of window resize signal is generated
void terminal_util()
{
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_sz);
	rows=term_sz.ws_row;
	cols=term_sz.ws_col;
	rows--;
	if(signal(SIGWINCH,term_sz_ch)==SIG_ERR)
		printf("Signal error!\n");
}

//This function is executed at the start 
//This will set the mode bit to normal mode, retreives the current directory, user home
//Changes the mode of the terminal to raw/non-canonical mode
void on_start()
{
	mode=0;
    getcwd(app_root,1000);
	struct passwd *cu = getpwuid(getuid());
	usr_home=cu->pw_dir;
    def_term=my_term;
	tcgetattr(0,&my_term);
    def_term=my_term;			//Stores the default settingd of terminal to change before exiting
	my_term.c_lflag&=~ICANON;	//Disable canonical mode
	my_term.c_lflag&=~ECHO;		//Disable echo
	my_term.c_cc[VMIN] = 1;		//Data is read when atleast on charater is entered
    my_term.c_cc[VTIME] = 0;	//No timing constraint to read input
    tcsetattr(STDIN_FILENO,TCSAFLUSH, &my_term);
	printf("\033[H\033[J");
    printf("\033[%d;%dH",curx,cury);
}

//Reads the input and peforms corresponding action in normal mode
void normal_mode()
{
    while(1)
    {
		char ch = cin.get(); 
		if(rsflag==1) //If window is resized, content will be adjusted
		{
			rsflag=0;
			display_content(rows,cont_off);
			//printf("here\n");
		}
		if (ch==ESC) //To detect Left, Right, Up and down keys. The first character is ESC in these cases
		{
			char r1,r2;
			r1=cin.get();
			r2=cin.get();
			if (r1==BR) 
			{
				if (r2==UP) //The cursor will go one position up
	 			{
					if(curx>1)
						goup();
				}
				else if (r2==DOWN) //The cursor will go one position down but won't move if it is in the second line form the last
				{
					if(curx<cont_sz && curx<rows)
						godown();
				}
				else if (r2==RIGHT) //To go to the directory visited after the current directory, if present
				{
					if(!trace_front.empty())
					{
						trace_back.push(cur_dr);
						string goto_path=trace_front.top();
						trace_front.pop();
						opendirect(goto_path);
						/*clearline(rows-3);
						cout<<cur_dr;*/
					}
				}
				else if (r2==LEFT) //To go to the directory visited after the current directory, if present
				{
					if(!trace_back.empty())
					{
						trace_front.push(cur_dr);
						string goto_path=trace_back.top();
						trace_back.pop();
						opendirect(goto_path);
						/*clearline(rows-3);
						cout<<cur_dr;*/
					}
				}
			}
		}
		else if(ch==ENTER)	//Enter will open the file in vi or open and display the content of directory
		{
			openfd();
		}
		else if(ch==127)	//Backspace will move to the parent directory of the current one
		{
			if(cur_dr!="/")
			{
				trace_back.push(cur_dr);
				while(!trace_front.empty()) //Clears the front stack when visiting the parent directory
					trace_front.pop();
				string temp;
				int pos=cur_dr.find_last_of('/');
				if(pos==0) //If the parent/current directory is root(/) change the current directory to root
				{
					cur_dr="/";
				}
				else //Extracts the parent directory path from the current directory path
				{
					temp=cur_dr.substr(0,pos);
					cur_dr=temp;
				}
				opendirect(cur_dr); //Move to the parent directory and display content
			}
		}
		else if(ch=='k' || ch=='K')  //Scroll up in case of overflow and if scroll down is done already
		{
			if(cont_sz>rows && cont_off>0)
			{
				cont_off--;
				display_content(rows,cont_off);
				printf("\033[%d;%dH",++curx,cury); //Position the cursor on the same entry
			}
		}
		else if(ch=='l' || ch=='L')   //Scroll down in case of overflow and if scroll up is done already
		{
			if(cont_sz-cont_off-rows>0)
			{
				cont_off++;
				display_content(rows,cont_off);
				printf("\033[%d;%dH",--curx,cury); //Position the cursor on the same entry
			}
		}
		else if(ch=='h')	//Go to user home directory
		{
			opendirect(usr_home);
			trace_back.push(cur_dr);	//Push the current directory in the trace back stack
			while(!trace_front.empty())		//Clear the front stack 
				trace_front.pop();			

		}
		else if(ch==':')	//Go to command mode
		{
			command_mode();
		}
		else if(ch=='q')	//Quit the file explorer
		{
			printf("\033[H\033[J");
			break;
		}
    }
}

void godown() //Move cursor one position down
{
	printf("\033[%d;%dH",++curx,cury);
}

void goup() //Move cursor one position up
{
	printf("\033[%d;%dH",--curx,cury);
}

void goleft() //Move cursor one position left
{
	printf("\033[%d;%dH",curx,--cury);
}

void goright() //Move cursor one position right
{
	printf("\033[%d;%dH",curx,++cury);
}

void gotop()  //Move the cursor to top left corner
{
	printf("\033[%d;%dH",1,1);
}

void goend() //Move the cursor to bottom left corner
{
	printf("\033[%d;%dH",rows+1,1);
}

void goline(int pos) //Move cursor to given line
{
	printf("\033[%d;%dH",pos,1);
}

//Opens the directory passed to it and displays the content
void opendirect(string cpath)
{
	if(chdir(cpath.c_str())!=0)
	{
		printf("Can't open directory");
		return;
	}
	cur_dr=cpath;
	clear;
	get_content(cpath.c_str());
	display_content(rows,0);;
	gotop();
}

//Opens the file in editor
void openfd()
{
	int entry=cont_off+curx-1;
	string cur_name=cur_files[entry].file_name;
	string cpath=cur_dr+"/"+cur_name;
	char type=cur_files[entry].perm[0];
	printf("\033[%d;%dH\033[K",rows-1,1);	
	//cout<<entry<<" "<<type<<" "<<cpath;
	if(type=='d')
	{
		trace_back.push(cur_dr);
		while(!trace_front.empty()) //Clears the front stack when a new directory is opened
			trace_front.pop();
		opendirect(cpath);
	}
	else if(type=='-')
	{
		int child=fork();
		if(child==0)
		{
    		execlp("xdg-open", "xdg-open", cpath.c_str(), NULL);
			exit(0);
		}
	}
}

//Restore the terminal settings
void restore_to_default()
{
	clear;
	tcsetattr(STDIN_FILENO,TCSAFLUSH, &def_term);
	exit(0);
}

//Resolves the path from relative to absolute
string get_path(string name)
{
	string new_curdir="";
    if(name[0]=='/')	//If the path starts with / append / to result
        new_curdir="/";
	else if(name[0]=='~') //If the path starts with ~ append home directory path to result
	{
		name=name.substr(2);
		if(name.size()>0)
			name=name.substr(1);
		new_curdir=usr_home;
		if(name.size()==0) 	//If the path is just ~ then return result
			return new_curdir;
	}
	else if(name[0]!='.')	//If the path starts with anything other than /,~,. then append current directory
	{
		new_curdir=cur_dr;
	} 
	char *sparts=strtok((char *)name.c_str(),"/"); //Split the path to tokens seperated by /
	while(sparts!=NULL)
	{	
		string part=sparts;
		if(part[0]=='.' && part[1]=='.') //If the current token is .. then extract parent dir of result
		{
			if(new_curdir=="")
			{
				new_curdir=cur_dr;
			}
			new_curdir=new_curdir.substr(0,new_curdir.find_last_of("/"));
            if(new_curdir.size()==0)
                new_curdir="/";
		}
		else if(part[0]=='.') //If . is at the start assign cur dir to result else ignore
		{
			if(new_curdir=="")
				new_curdir=cur_dr;
		}
		else //If the token starts with a character append the token to result
		{
            if(new_curdir=="" || new_curdir=="/")
                new_curdir+=part;
            else
			    new_curdir=new_curdir+"/"+part;
		}
        sparts=strtok(NULL,"/"); //Get the next token
	}
	return new_curdir;
}

//Splits the given string separated with space delimiter and returns the vector with individual tokens
vector <string> split(string str)
{
	vector <string> res;
	string cur="";
	int i=0;
	while(i<str.size())
	{
		if(str[i]==' ' && cur!="")
		{
			res.push_back(cur);
			cur="";
		}
		else if(str[i]!=' ')
		{
			cur+=str[i];
		}
		i++;
	}
	if(cur!="")
		res.push_back(cur);
	return res;
}

//Copies a file to dest
int copy_file(string file,string dest)
{
    struct stat n;
    //Get the metadata of given file
    if(stat(file.c_str(),&n)==-1) 
    {
        return -1;
    }
    int src,dt;

	//Open the source file in read mode
    if((src=open(file.c_str(),O_RDONLY))==-1)
    {
        return -1;
    }

	//Open/Create dest file in Write and Append mode with full permissions
    dt=open(dest.c_str(),O_WRONLY|O_APPEND|O_CREAT,00777);
    char buf;
    long long size=0;

	//Read character by character until the last character is read
    while((size=read(src,&buf,1)>0))
    {
		//Write character by character
        if(write(dt,&buf,size)==-1)
        {
            return -1;
        }
    }

	//Change the permissions, user and group details of copied file
    if(chmod(dest.c_str(),n.st_mode)==-1)
    {
        return -1;
    }
    return 0;
}

//Copy a given directory to destination path
int copy_dir(string source,string dest)
{
    dest=dest+"/"+source.substr(source.find_last_of("/")+1);

	//Create the source directory in the dest path
    if(create_dir(dest)==-1)
    {
        return -1;
    }

	//Open the source directory
    DIR *d;
    if((d=opendir(source.c_str()))!=NULL)
    {
        struct dirent *cur;

		//Read the content of source directory one by one
        while((cur=readdir(d))!=NULL)
        {
			//If the entry is . or .. then ignore it
            if(cur->d_name[0]=='.')
                continue;
            string cur_src=source+"/"+cur->d_name;

			//If the entry is a directory, recursively call copy_dir function
            if(cur->d_type==DT_DIR)
            {
                if(copy_dir(cur_src,dest)==-1)
                {
                    return -1;
                }
            }
			//If the entry is a file, call the copy_file function
            else
            {
                if(copy_file(cur_src,dest+"/"+cur->d_name)==-1)
                {
                    return -1;
                }
            }
        }
    }
    else
        return -1;
    struct stat curdir;

	//Read the metadata of the directory
    if(stat(source.c_str(),&curdir)==-1)
    {
        return -1;
    }
    else
    {
		//Change the mode of directory
        if(chmod(dest.c_str(),curdir.st_mode)==-1)
        {
            return -1;
        }
    }
    return 0;
}

//This function will copy the parameters passed to copy command
void copy(string params)
{
    int pos=params.find_last_of(" ");
    string dest=params.substr(pos+1);
    params=params.substr(0,pos);
    dest=get_path(dest);

	//Split the parameters 
    vector <string> entries=split(params);

	//Copy each entry into the destination
    for(string entry:entries)
    {
        //if(entry[0]=='.')
        //        continue;
        string fname=entry;
        entry=get_path(entry);
        struct stat cur;
		//Read the metadata of the parameter
        if(stat(entry.c_str(),&cur)!=-1)
        {
			//If the parameter is a directory, call copy_dir function
            if(S_ISDIR(cur.st_mode))
            {
                if(copy_dir(entry,dest)==-1)
                {
                    printf("Error while copying");
                    return;
                }
            }
			//If the parameter is a directory, call copy_file function
            else
            {
                if(copy_file(entry,dest+"/"+fname)==-1)
                {
                    printf("Error while copying");
                    return;
                }
            }
        }
        else
        {
            printf("Error while copying %d",errno);
            return;
        }
    }

}

//Moves the file from source to destination
int move_file(string source,string dest)
{
    if(rename(source.c_str(),dest.c_str())==-1)
        return -1;
    else
        return 0;
}

//Moves the directory from source to destination
int move_dir(string source,string dest)
{
    dest=dest+"/"+source.substr(source.find_last_of("/")+1);

	//Create the source directory in the dest path
    if(create_dir(dest)==-1)
    {
        return -1;
    }
	//Open the source directory
    DIR *d;
    if((d=opendir(source.c_str()))!=NULL)
    {
        struct dirent *cur;
		//Read the content of source directory one by one
        while((cur=readdir(d))!=NULL)
        {
			//If the entry is . or .. then ignore it
            if(cur->d_name[0]=='.')
                continue;
            string cur_src=source+"/"+cur->d_name;

			//If the entry is a directory, recursively call copy_dir function
            if(cur->d_type==DT_DIR)
            {
                if(move_dir(cur_src,dest)==-1)
                {
                    return -1;
                }
            }
			//If the entry is a file, call the copy_file function
            else
            {
                if(move_file(cur_src,dest+"/"+cur->d_name)==-1)
                {
                    return -1;
                }
            }
        }
    }
    else
        return -1;
    struct stat curdir;
	//Read the metadata of the directory
    if(stat(source.c_str(),&curdir)==-1)
    {
        return -1;
    }
    else
    {
		//Change the mode of directory
        if(chmod(dest.c_str(),curdir.st_mode)==-1)
        {
            return -1;
        }
    }

	//Remove the source directory
    if(rmdir(source.c_str())==-1)
		return -1;
    return 0;
}

//This function will move the parameters passed to move command
void move(string params)
{
    int pos=params.find_last_of(" ");
    string dest=params.substr(pos+1);
    params=params.substr(0,pos);
    dest=get_path(dest);

	//Split the parameters 
    vector <string> entries=split(params);

	//Copy each entry into the destination
    for(string entry:entries)
    {
        if(entry[0]=='.')
                continue;
        string fname=entry;
        entry=get_path(entry);
        struct stat cur;
		//Read the metadata of the parameter
        if(stat(entry.c_str(),&cur)!=-1)
        {
			//If the parameter is a directory, call copy_dir function
            if(S_ISDIR(cur.st_mode))
            {
                if(move_dir(entry,dest)==-1)
                {
                    printf("Error while Moving");
                    return;
                }
            }
			//If the parameter is a directory, call copy_file function
            else
            {
                if(move_file(entry,dest+"/"+fname)==-1)
                {
                    printf("Error while copying");
                    return;
                }
            }
        }
        else
        {
            printf("Error while copying %d",errno);
            return;
        }
    }

}

//This function renames thye file
void rename_file(string remstr)
{
	clearcurline;
	goend();
	int pos=remstr.find_last_of(" ");
	string newf=remstr.substr(pos+1);
	remstr=remstr.substr(0,pos);
	string oldf=get_path(remstr);
	newf=oldf.substr(0,oldf.find_last_of("/"))+"/"+newf;
	
	//Rename the file from old string to new string
	if(rename(oldf.c_str(),newf.c_str())==-1)
	{
		printf("Error while renaming the file");
	}
	else
	{
		printf("File renamed successfully");
	}
}

//Create the given filr
int create_file(string fname)
{
	fname=get_path(fname);
	//cout<<fname;
	if(creat(fname.c_str(),00666)==-1)
		return -1;
	else
		return 0;
}

//Create the given directory
int create_dir(string dname)
{
	dname=get_path(dname);
	if(mkdir(dname.c_str(),00777)==-1)
		return -1;
	else
		return 0;
}

//Delete the given file
int del_file(string fpath)
{
	fpath=get_path(fpath);
	if(unlink(fpath.c_str())==-1)
		return -1; 
	else
		return 0;
}

//Delete the directory
int del_dir(string dpath)
{
	clearcurline;
	goend();
	dpath=get_path(dpath);

	//Open the directory
	DIR *dptr;
	char xyz[1000];
	struct dirent *cur=(struct dirent *)malloc(sizeof(struct dirent));
	dptr=opendir(dpath.c_str());
	if(dptr!=NULL)
	{
		//Read the content of source directory one by one
		while((cur=readdir(dptr))!=NULL)
		{
			//If the entry is . or .. then ignore it
			if(cur->d_name[0]!='.')
			{
				string fpath=dpath;
				fpath+="/";
				fpath+=cur->d_name;
				//If the entry is a directory, recursively call del_dir function
				if(cur->d_type==DT_DIR)
				{
					if(del_dir(fpath.c_str())==-1)
					{
						return -1;
					}
				}
				//If the entry is a file, call the del_file function
				else
				{
					if(del_file(fpath.c_str())==-1)
					{
						return -1;
					}
				}
			}
		}
	}
	else
		return -1;
	closedir(dptr);
	//Remove the empty directory at the end
	if(rmdir(dpath.c_str())==-1)
		return -1;
	else
		return 0;
}

//Change the current working directory to given path
void goto_loc(string path)
{
	clearcurline;
	goend();
	path=get_path(path);
	cur_dr=path;
	//Change the cur working dir
	if(chdir(path.c_str())==-1)
	{
		printf("Not able to change directory");
	}
	else
	{
		//Read the metadata of entries in new cur directory
		get_content(path.c_str());
		cont_off=0;
		//Display the content
		display_content(rows,cont_off);
		printf("Changed directory successfully");
		cmd_cur_dir=path;

	}
}

//This will search the given file recursively in the current directory
int search(string source,string dest)
{
    DIR *d;
	int ret=0;
	//Open the source directory
    if((d=opendir(dest.c_str()))!=NULL)
    {
        struct dirent *cur;
		//Read the content of source directory one by one
        while((cur=readdir(d))!=NULL)
        {
			//If the entry is . or .. then ignore it
            if(cur->d_name[0]=='.')
                continue;
			
			//Compare the given entry name and current entry name, if it's a match then return
            if(source.compare(cur->d_name)==0)
            {
                return 1;
            }

			//If the entry is a directory, recursively call search function
            if(cur->d_type==DT_DIR)
            {
				int res=search(source,dest+"/"+cur->d_name);
                if(res==1)
                {
                    return 1;
                }
				if(res==-1)
					ret=res;
            }
        }
    }
    else
        return -1;
    return ret;
}

//This is the driver code of command mode
void command_mode()
{
	//Store the directory details while switching from normal mode to command mode
	string sw_dir=cur_dr;
	int sw_off=cont_off;
	int sw_cur=curx;
	cmd_cur_dir=cur_dr;
	mode=1;
	goline(rows);
	clearcurline;
	//Print Command mode in the second last line
	printf("***Command Mode***");
	//Goto last line
	goend();
	int flag=0;
	clearcurline;
	//Continuously read characters until q or ESC is encountered
	while(1)
	{
		char ch;
		ch=cin.get();
		//If q is pressed, restore the terminal settings and exit
		if(ch=='q')
		{
			restore_to_default();
		}
		//If ESC is pressed go to normal mode
		else if(ch==27 && ch!=127)
		{
			cur_dr=sw_dir;
			cont_off=sw_off;
			curx=sw_cur;
			clearcurline;
			display_content(rows,cont_off);
			goline(curx);
			normal_mode();
		}
		//If backspace is pressed, do nothing
		else if(ch==127)
		{
			continue;
		}
		//if enter is pressed, do nothing
		else if(ch==10)
		{
			clearcurline;
			goend();
			//printf("Enter valid command");
		}
		//Read characters until Enter is encountered
		else
		{
			clearcurline;
			goend();
			string cmd="";
			char prev=ch;
			while(ch!=10)
			{
				int sz=cmd.size();
				//if the command is cleared and q is entered, then it will exit
				if(ch=='q' && sz==0)
				{
					restore_to_default();
				}
				//if command is cleared and ESC is pressed, then goto normal mode
				if(ch!=127 && ch==27 && sz==0)
				{
					clearcurline;
					display_content(rows,cont_off);
					goline(curx);
					normal_mode();
				}
				//If Backspace is pressed then clear the last entered character and display the string
				if(ch==127)
				{
					if(sz>0)
					{
						cmd.erase(sz-1);
						clearcurline;
						goend();
						printf("%s",cmd.c_str());
					}
					else 
					{
						flag=1;
						break;
					}
					ch=cin.get();
				}
				else
				{
					//Print the entered character for user visbility
					printf("%c",ch);
					//If two or more spaces are entered store only one space
					if(ch!=' ' || (ch==' ' && prev!=' '))
						cmd+=ch;
					prev=ch;
					//Read next character
					ch=cin.get();
				}
			}
			//When everything is cleared and backspace is entered again flag will be 1
			//and control comes here, do nothing in that case
			if(flag==1)
			{
				flag=0;
				break;
			}
			//Remove the trailing spaces
			while(cmd[cmd.size()-1]==' ')
				cmd.erase(cmd.size()-1);

			//Find the first space
			int pos=cmd.find(" ");
			//Each command will contain atleast 4 characters, so if space is found after 4 charcters
			//check which command is entered
			if(pos>3)
			{
				string cur_cmd=cmd.substr(0,pos);
				string params=cmd.substr(pos+1);
				//Call copy function
				if(cur_cmd=="copy")
				{
					clearcurline;
					goend();
					copy(params);
				}
				//Call move function
				else if(cur_cmd=="move")
				{
					clearcurline;
					goend();
					move(params);
				}
				//Call rename function
				else if(cur_cmd=="rename")
				{
					clearcurline;
					goend();
					rename_file(params);
				}
				//Call create_file function
				else if(cur_cmd=="create_file")
				{
					clearcurline;
					goend();
					if(create_file(params)==0)
						printf("File Created Successfully");
					else
						printf("Error while creating file");
				}
				//Call create_dir function
				else if(cur_cmd=="create_dir")
				{
					clearcurline;
					goend();
					if(create_dir(params)==0)
						printf("Direc Created Successfully");
					else
						printf("Error while creating direc");
				}
				//Call delete_file function
				else if(cur_cmd=="delete_file")
				{
					clearcurline;
					goend();
					if(del_file(params)==0)
						printf("File deleted");
					else
						printf("Unable to delete the file");
				}
				//Call delete_dir function
				else if(cur_cmd=="delete_dir")
				{
					clearcurline;
					goend();
					if(del_dir(params)==0)
						printf("Directory deleted");
					else
						printf("Error while deleting");
				}
				//Call goto function
				else if(cur_cmd=="goto")
				{
					clearcurline;
					goto_loc(params);
					goend();
					//cout<<"goto";
				}
				//Call search function
				else if(cur_cmd=="search")
				{
					clearcurline;
					goend();
					int res=search(params,cmd_cur_dir);
					if(res==1)
						printf("True");
					else if(res==0)
						printf("False");
					else
						printf("Error occurred while searching");
				}
				//Else print error message
				else
				{
					clearcurline;
					goend();
					printf("Invalid command	%s",cmd.c_str());
				}
			}
			//If first space is found before 3 characters then it is an invalid command
			else
			{
				clearcurline;
				goend();
				printf("Invalid command	%s",cmd.c_str());
			}
			
		}
	}
}

//Main function
int main()
{
	//Read the size of the terminal and add a function to handle window resize event
	terminal_util();
    //Set the terminal in raw mode and read current dir, home dir values and store them
	on_start();
    //Read the content of current directory
	get_content(app_root);
	cur_dr=app_root;
	//Display the content of current directory
    display_content(rows,0);
	//Change the mode of terminal to normal mode
	mode=0;
    normal_mode();
	//Reset the terminal to default settings
    tcsetattr(STDIN_FILENO,TCSAFLUSH, &def_term);
	return 0;
}