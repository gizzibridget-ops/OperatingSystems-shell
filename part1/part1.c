#include <stdio.h> 
#include <unistd.h> 
#include <sys/utsname.h>

int main() 
{
	getpid(); 
	getuid(); 
	getgid(); 
	struct utsname buf; 
	uname(&buf); 
	sync(); 
	return 0; 
}

