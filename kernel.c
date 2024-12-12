//kernel.c
//Created By: Thomas Rose

//ProjectB Functions
void printString(char*);
void printChar(char*);
char* readString(char* userInputBuffer);
char* readSector(char*, int);
void handleInterrupt21(int ax, char* bx, int cx, int dx);
//ProjectC Functions
int directoryExists(char* directory_sector, int* file_entry, char* possible_filename);
void readFile(char* filename, char* output_buffer, int* sectorsRead);
void executeProgram(char* filename);
void terminate();
//ProjectD Functions
void writeSector(char* buffer, int sector);
void deleteFile(char* filename);
void writeFile(char* dataBuffer, char* fileName, int sectorsRequired);
//ProjectE Functions
void handleTimerInterrupt(int segment, int sp);
void killProcess(int pid);

int processWaitingOn[9];


int processActive[8];
int processStackPointer[8];
int currentProcess;
//////////////////////////////////////////////////////////////////////////////////////////
//main function
void main(){
	int i,j;
	for(i=0;i<8;i++)
		processActive[i]=0;
	for(j=0;j<8;j++)
		processStackPointer[j]=0xff00;
	makeInterrupt21();
	currentProcess=-1;
	interrupt(0x21,4,"shell");
	makeTimerInterrupt();
	while(1);
}
//////////////////////////////////////////////////////////////////////////////////////////
//Prints string to terminal.
void printString(char* chars){
    int i=0;
    while (chars[i] != 0x0){ //0x0 signifies the last character in the string.
        char al = chars[i]; //AL register is the place holder for each character.
        char ah = 0xe; //AH register is always 0xe.
        int ax = ah*256 + al; //AX is always equal to AH*256+AL.
        interrupt(0x10,ax,0,0,0);//0,0,0 are registers BX,CX,DX but are not used.
        ++i;
    }
}

//Prints a single character to the terminal.
void printChar(char* c){    
    char al = c;
    char ah = 0xe;
    int ax = ah*256 + al;
    interrupt(0x10,ax,0,0,0);
}

//Takes input from the screen and sends it back out (like echo command).
char* readString(char* userInputBuffer)
{
    char currentChar = 0x0; 
    int bufferIndex = 0;    

    while (bufferIndex < 79) {
        currentChar = (char) interrupt(0x16, 0, 0, 0, 0);

        if (currentChar == 0xd){break;}
		if (currentChar == 0x8 && bufferIndex == 0){continue;}

        if (currentChar == 0x8) {
            char blankSpace = ' '; 
            interrupt(0x10, 0xe * 256 + currentChar, 0, 0, 0); 
            interrupt(0x10, 0xe * 256 + blankSpace, 0, 0, 0);  
            interrupt(0x10, 0xe * 256 + currentChar, 0, 0, 0); 
            --bufferIndex;
            continue;
        }

        userInputBuffer[bufferIndex] = currentChar;
        interrupt(0x10, 0xe * 256 + currentChar, 0, 0, 0); 
        ++bufferIndex; 
    }

    userInputBuffer[bufferIndex] = 0xd;    
    userInputBuffer[bufferIndex + 1] = 0x0; 

    interrupt(0x10, 0xe * 256 + 0xd, 0, 0, 0); 
    interrupt(0x10, 0xe * 256 + 0xa, 0, 0, 0); 

    return userInputBuffer;
}

//Reads from a file and outputs it to the screen.
char* readSector(char* buffer, int sector){
    int ah = 2; //Tells BIOS to read a sector rather than write.
    int al = 1; //Leave as 1, its the number of sectors to read.
    int ax = ah * 256 + al;

    char* bx = buffer; //address where the data should be stored to.
    
    int ch = 0; //track number.
    int cl = sector + 1; //relative sector number.
    int cx = ch * 256 + cl; 

    int dh = 0; //head number.
    int dx = dh * 256 + 0x80;

    interrupt(0x13, ax, bx, cx, dx);
    
    return buffer;
}

//Our own defined interrupt that can be called within kernel.c.
void handleInterrupt21(int ax, char* bx, int cx, int dx){
    switch(ax){ //AX is the # that determines which function to run.
        case 0: printString(bx); break; //if ax is 0, it'll printString.
        case 1: readString(bx); break; //if ax is 1, it'll readString.
        case 2: readSector(bx, cx); break; //if ax is 2, it'll read sector.
        case 3: readFile(bx, cx, dx); break; //if ax is 3, it'll readFile.
        case 4: executeProgram(bx); break; //if ax is 4, it'll executeProgram.
        case 5: terminate(); break; //if ax is 5, it'll terminate.
        case 6: writeSector(bx, cx); break; //If ax is 5, it'll writeSector.
        case 7: deleteFile(bx); break; //if ax is 7, it'll deleteFile.
        case 8: writeFile(bx, cx, dx); break; //if ax is 8, it'll writeFile.
        case 9: killProcess(bx); break; //if ax is 9, then killProcess.
		default: printString("ERROR: AX is invalid."); break; //if ax isn't anything above, it'll print and error.
    }
}

//Will return either 1 or 0 (true or false) if a file exists in the directory.
int directoryExists(char* directory_sector, int* file_entry, char* possible_filename){
    int i = 0; //counter
    int letters = 0; //counter for correct number of letters in the loop. 
    
    //Steps through the directory in increments of 32.
    for(*file_entry=0; *file_entry<512; *file_entry+=32){
        //Compares the first 6 letters in the filename.
        while(i<6){
            //If the letter isn't the same.
            if(directory_sector[*file_entry + i] != possible_filename[i]){
                break;
            }
            //If the letter is the same.
            else{
                ++letters;
            }
            ++i;
        }
        //If all 6 letters are the same, return true.
        if(letters == 6){
            return 1;
        }
    }
    //If all 6 letters don't match, return false.
    return 0;
}

//Reads from a file and outputs it to the screen.
void  readFile(char* filename, char* output_buffer, int* sectorsRead){
    int i = 0; //counter
    char directory_sector[512]; //load directory sector into 512 byte character array.
    int file_entry = 0;
    int* param = &file_entry;
    *sectorsRead = 0;
    readSector(directory_sector, 2); //Load the directory sector

    //If the filename exists.
    if(directoryExists(directory_sector, param, filename) == 1){ 
        while(directory_sector[*param + i] != 0){  //Loop through file's sector entries
            readSector(output_buffer, directory_sector[*param + 6 + i]); //Read each sector
            output_buffer += 512; //Move buffer pointer to the next sector
            ++*sectorsRead; //Increase sector count
            ++i;
        }
    } else {
        *sectorsRead = 0; //If the file doesn't exist, set sectorsRead to 0
    }
}

//Will take the name of a program, use readFile to locate it, and it'll execute it.
void executeProgram(char* program){
    int index = 0, processIndex = 0;
    int segmentAddr = 0;
    int kernelDataSeg = 0;
    int sectorsRead;
    char programBuffer[13312];
    
    sectorsRead = 0;
    interrupt(0x21, 3, program, programBuffer, &sectorsRead); //Reads the program from memory.
    
    if(sectorsRead != 0) {
        kernelDataSeg = setKernelDataSegment(); //Sets the kernel data segment.
        
        //Finds an available process slot.
        while(processActive[processIndex] != 0) {
            processIndex++;
            if(processIndex == 8)
                processIndex = 0; //Wraps around if the process index reaches the limit.
        }

        restoreDataSegment(kernelDataSeg); //Restores the original data segment.

        segmentAddr = (processIndex + 2) * 0x1000; //Calculates memory segment for the new process.
        
        //Puts the program's data into memory.
        for(index = 0; index < 13312; index++) {
            putInMemory(segmentAddr, index, programBuffer[index]);
        }

        initializeProgram(segmentAddr); //Initializes the program for execution.

        kernelDataSeg = setKernelDataSegment(); //Sets the kernel data segment again.
        processStackPointer[processIndex] = 0xff00; //Sets the stack pointer for the new process.
        processActive[processIndex] = 1; //Marks the process as active.
        restoreDataSegment(kernelDataSeg); //Restores the original data segment.
    } else {
        return; //Returns if no sectors were read (program not found).
    }
}

//Will make an interrupt 0x21 call to reload and execute shell.
void terminate(){
	int dataseg=0;
	dataseg=setKernelDataSegment();
	processActive[currentProcess]=0;
	restoreDataSegment(dataseg);
	while(1);
}

//Writes to a sector on disk.
void writeSector(char* buffer, int sector){
    int ah = 3;
    int al = 1; //# of sectors to write.
    int ax = ah * 256 + al;
    
    char* bx = buffer; 

    int ch = 0; //Track #.
    int cl = sector + 1; //Relative sector #.
    int cx = ch * 256 + cl;

    int dh = 0; //Head #.
    int dx = dh * 256 + 0x80;

    interrupt(0x13, ax, bx, cx, dx);
}

//Deletes a file in the directory.
void deleteFile(char* filename){
    char directory_sector[512];
    int file_entry;
    int i = 0;
    char buffer[512]; 
    
    readSector(directory_sector, 2);

    //If the filename exists.
    if(directoryExists(directory_sector, &file_entry, filename) == 1){
        //Go through file entry and delete its entry in directory.
        for(i=0; i<6; i++){
            directory_sector[file_entry + i] = 0x0;
        }
    }
    
    //Write back to sector.
    writeSector(directory_sector, 2);
}

//Writes a file to disk by writing to sectors.
void writeFile(char* dataBuffer, char* fileName, int sectorsRequired){
    int i = 0;
    char directory_sector[512];
    int file_entry;
    readSector(directory_sector, 2);
    //write the name of the file into the directory sector.
    for(i = 0; i < 6; i++) {
        directory_sector[file_entry + i] = fileName[i];
    }

    writeSector(directory_sector, 2);
}

//Handles the timer interrupt, switching between processes.
void handleTimerInterrupt(int segment, int sp){
    int kernelDataSeg = 0;
    int nextProcessIndex = 0;

    kernelDataSeg = setKernelDataSegment(); //Sets the kernel data segment to access process-related data.

    if(currentProcess != -1) {
        processStackPointer[currentProcess] = sp; //Saves the stack pointer of the current process.
    }

    nextProcessIndex = currentProcess; //Stores the current process index temporarily.
    nextProcessIndex++; //Increments to find the next process.

    if(nextProcessIndex == 8) {
        nextProcessIndex = 0; //Wraps around if the process index reaches the limit.
    }

    //Finds the next active process.
    while(processActive[nextProcessIndex] != 1) {
        nextProcessIndex++; //Increments to check the next process.
        if(nextProcessIndex == 8) {
            nextProcessIndex = 0; //Wraps around if the process index reaches the limit.
        }
    }

    currentProcess = nextProcessIndex; //Sets the next process as the current process.
    segment = (currentProcess + 2) * 0x1000; //Calculates the segment for the new process.
    sp = processStackPointer[currentProcess]; //Retrieves the stack pointer for the new process.

    restoreDataSegment(kernelDataSeg); //Restores the kernel data segment to its original state.
    returnFromTimer(segment, sp); //Returns control to the next process.
}

void killProcess(int pid){    
    int i;
    int dataSeg = setKernelDataSegment();
    processActive[pid] = 0;

    for(i=0; i < 9; i++){
        if(processWaitingOn[i] == pid){
            processActive[i] = 1;
        }
    }

    restoreDataSegment(dataSeg);
}

