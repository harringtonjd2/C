#include <stdio.h>
#include <string.h>

int main() {

	char str[50];
	printf("Enter string to be tokenized: ");
	fgets(str, 49, stdin);
	printf("\n");
	char* token;
	
	token = strtok(str, " ");
	printf("%s\n", token);
	while ( (token = strtok(NULL, " ")) != NULL ) // After first use,
		 printf("%s\n", token);		    // NULL is used to get the 
						 // previous string
	return 0;
}
		
