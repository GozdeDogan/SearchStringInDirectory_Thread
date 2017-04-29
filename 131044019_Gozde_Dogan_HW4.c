////////////////////////////////////////////////////////////////////////////////
// Gozde DOGAN 131044019
// Homework 4
// c dosyasi
// 
// Description:
//      Girilen directory icerisindeki her file'da yine girilen stringi aradim.
//      String sayisini ekrana yazdirdim
//      Her buldugum string'in satir ve sutun numarasini buldugum, 
//      process idsi ve thread idsi ile birlikte log.txt dosyasina yazdim.
//      Her file dan sonra o file da kac tane string oldugunu da 
//      log dosyasina yazdim.
//      Her directory icin process(fork olusturuldu) olusturuldu. 
//      Directoryler icindeki her file icin de thread olusturulur.
//      Islem yapilirken directory-directory haberlesmesi fifo, 
//      file-directory haberlesmesi pipe ile saglandi.
//
// Ekran Ciktisi:
//      1. bulunan toplam string sayisi
//      2. toplam directory sayisi (parametre olarak girilen directory dahil)
//      3. toplam file sayisi
//      4. toplam line sayisi
//      5. olusturulan process sayisi
//      6. olusturulan toplam thread sayisi
//      7. arama isleminin tamamlanma suresi
//      8. cikis nedeni
//
// Important:
//      Kitaptaki restart.h kutuphanesi kullaildi.
//      restart.h ve restart.c dosyalari homework directory'sine eklendi.
//      istenilen degerlerin bulunabilmesi icin temp dosyalari olusturuldu
//      islemler bitince dosyalar da kaldirildi.
//      log.txt ise log file!
//
// References:
//      1. DirWalk fonksiyonunun calisma sekli icin bu siteden yararlanildi.
//      https://gist.github.com/semihozkoroglu/737691
//      
//      2. thread fonksiyonlarinin calisma sekli icin.
//      https://computing.llnl.gov/tutorials/pthreads/
//      
//      3. semaphore fonksiyonlarinin kullanimi bu siteden incelendi.
//      http://www.csc.villanova.edu/~mdamian/threads/posixsem.html
//      
//      4. sinyal incelemesi
//      https://www.tutorialspoint.com/c_standard_library/c_function_signal.htm
//         
//      5. time hesabi
//      https://users.pja.edu.pl/~jms/qnx/help/watcom/clibref/qnx/clock_gettime.html
//
////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////// LIBRARY ////////////////////////////////////
#include "131044019_Gozde_Dogan_HW4.h"

////////////////////// Global Variables ////////////////////////////////////////
int iNumOfLine = 0;
int iLengthLine = 0;

FILE *fPtrInFile = NULL;

char *sSearchStr = NULL;
int iSizeOfSearchStr = 0;

FILE *fPtrWords;
FILE *fPtrFiles;
FILE *fPtrLines;;
FILE *fPtrThreads;

char pathWords[MAXPATHSIZE], pathDirs[MAXPATHSIZE], pathFiles[MAXPATHSIZE], pathLines[MAXPATHSIZE], pathThreads[MAXPATHSIZE];

threadData dataOfThread[MAXSIZE];
pthread_t threadArr[MAXSIZE];
int sizeOfThread = 0;

numOf numOfValues;
int iDirs = 0;
int iMaxThreads = 0;
int fifo2 = 0; //log dosyasi
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]){ 
    /////////////////////////////// VARIABLES //////////////////////////////////   
    int fifo1 = 0; //ana fifo 
    char s[MAXSIZE]; // fifoya yazialacak seyleri bu string'e atip yazdim
    int i=0; // tempFile.txt'den okuma yapabilmek icin kullandim
    char path[MAXPATHSIZE];
    char fifoBuffer[SIZEBUFFER]; //fifo1den okunan degerler burda tutulur
	int reads, writes; //fifo1den okunup fifo2(loga)ye yazmak icin okunan, yazilan byte degerlerini kontrol etmek icin tanimlandi
	
	struct timespec n_start, n_stop; //zaman hesabi icin kullanilan degiskenler   
    ////////////////////////////////////////////////////////////////////////////
    
    if (argc != 3) {
        printf("\n*******************************************************\n");
        printf ("\n    Usage>> ");
        printf("./grepTh \"searchString\" <directoryName>\n\n");
        printf("*******************************************************\n\n");
        return 1;
    }
   
    iSizeOfSearchStr = strlen(argv[1]);
    if(argv[1] != NULL)
        sSearchStr = (char*)calloc((strlen(argv[1])+1), sizeof(char));
    strncpy(sSearchStr, argv[1], ((int)strlen(argv[1])+1));
    
    //temp dosyalarinin kaldirilabilmesi icin bulunduklari path hesaplandi
    getcwd(path, MAXPATHSIZE);
    //temp dosyalari olusturulup, acildi
    openFiles(path);
    
    // semaphore olusturuldu
    sem_t semaphore;	
	sem_init(&semaphore, 0, 1);   
   
    //kullanilack ana fifo olusturuldu
    mkfifo(fifoName, (S_IRUSR | S_IWUSR));

    //kullanilacak ana fifo
	fifo1 = open(fifoName, O_RDWR | O_TRUNC );
	//log dosyasi
    fifo2 = open(LOGFILE, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    
    //islemin hesaplanmaya baslanildigi zaman hesaplandi
    if( clock_gettime( CLOCK_REALTIME, &n_start) == -1 ) {
      perror( "clock gettime" );
      return EXIT_FAILURE;
    }
    
    //directory ler arasi gezinme ve diger islemlerinin gerceklestirildigi fonksiyon
    numOfValues.iNumOfWords = DirWalk(argv[2], fifo1, &semaphore);
    
    //semaphore'un isleminin bitmesi beklendi
    //semaphore islemi bitene kadar kritik bolgeye girmesi engellendi
    sem_wait(&semaphore);
    
    //fifo1den degerler okundu ve bir string'e atildi.
    while(1){			
		reads = r_read(fifo1, fifoBuffer, SIZEBUFFER );			
		if(reads > 0){
			fifoBuffer[reads] = '\0';
			
			//fifo1den okunan degerler fifo2'ye(log'a) yazildi.		
			writes = r_write(fifo2, fifoBuffer, strlen(fifoBuffer));
					
			if(reads < SIZEBUFFER)
				break;
		}else{ 
		    //fifo1den okunacak deger yoksa islem dogru gerceklesememistir
		    //bu nedenle cikis yapildi
			fprintf(stderr, "  Exit Condition: due to error 1\n");
			sprintf(s, "  Exit Condition: due to error 1\n");
            write(fifo2, s, strlen(s));
			exit(1);
		}			
	}
	
	//olusturulan ana fifonun silinmesi icin gerekti!
	chdir("..");
	
	//semaphore'un islemi sonlandirildi
	sem_post(&semaphore);
	
	//ana fifo kaldirildi.
	unlink(fifoName);
    
    //gerekli degrler hesaplandi
    calculateNumOfValues();       
        
    //islemin bittigi zaman bulundu.
    if( clock_gettime( CLOCK_REALTIME, &n_stop) == -1 ) {
      perror( "clock gettime" );
      return EXIT_FAILURE;
    }
    
    //islemin gerceklesme suresi hesaplandi
    numOfValues.iTimeOperations = ((n_stop.tv_sec - n_start.tv_sec) *1000 / CLOCKS_PER_SEC) + ( n_stop.tv_nsec - n_start.tv_nsec );
    
    //acilan temp dosyalari kapatildi ve kaldirildi
    closeFiles();
    
    ///////////////////////////// print screen ////////////////////////////////
    printf("\n*******************************************************\n");
    printf("  Total number of strings found : %d\n", numOfValues.iNumOfWords);
    printf("  Number of directories searched: %d\n", numOfValues.iNumOfDirectories);
    printf("  Number of files searched: %d\n", numOfValues.iNumOfFiles);
    printf("  Number of lines searched: %d\n", numOfValues.iNumOfLines);
    printf("  Number of process created: %d\n", numOfValues.iNumOfProcess);
    printf("  Number of cascade threads created: %d\n", numOfValues.iNumOfCascadeThreads);
    printf("  Number of search threads created: %d\n", numOfValues.iNumOfThreads);
    printf("  Max # of threads running concurrently: %d\n", numOfValues.iNumOfMaxThreads);
    printf("  Total run time, in milliseconds: %.4f\n", numOfValues.iTimeOperations);
    printf("  Exit Condition: normal\n");
    printf("*******************************************************\n\n"); 

    
    /////////////////////////////// print log //////////////////////////////////
    sprintf(s, "\n*******************************************************\n");
    write(fifo2, s, strlen(s));
    sprintf(s, "  Total number of strings found : %d\n", numOfValues.iNumOfWords);
    write(fifo2, s, strlen(s));    
    sprintf(s, "  Number of directories searched: %d\n", numOfValues.iNumOfDirectories);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Number of files searched: %d\n", numOfValues.iNumOfFiles);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Number of lines searched: %d\n", numOfValues.iNumOfLines);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Number of process created: %d\n", numOfValues.iNumOfProcess);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Number of cascade threads created: %d\n", numOfValues.iNumOfCascadeThreads);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Number of search threads created: %d\n", numOfValues.iNumOfThreads);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Max # of threads running concurrently: %d\n", numOfValues.iNumOfMaxThreads);
    write(fifo2, s, strlen(s));
    sprintf(s, "  Total run time, in milliseconds: %.4f\n", numOfValues.iTimeOperations);
    write(fifo2, s, strlen(s)); 
    sprintf(s, "  Exit Condition: normal\n");
    write(fifo2, s, strlen(s)); 
    sprintf(s, "*******************************************************\n\n");
    write(fifo2, s, strlen(s));
    
    free(sSearchStr);
    close(fifo2);

    return 0;
}


///////////////////////////// Function Definitions /////////////////////////////

/**
 * Verilen directory icine girer.
 * directory icindeki her dosya icinde istenilen stringi arar.
 * Her directroy icin fork olusturur.
 * Her file icin thread olsuturur.
 * semaphore kullanildi!
 *
 * sNameDir: aranacak directory
 * fifo: kullanilan main fifo
 * semp: file'lar icin olusturulan semaphore(threadler)
 */
int DirWalk(const char *sNameDir, int fifo, sem_t *semp){    
    //////////////// variables /////////////////
    DIR *dir;
    struct dirent *mdirent = NULL; //recursive degisen directory
    struct stat buf; //file ve dir bilgisi ogrenmek icin

    pid_t pid; //forktan donen deger
    
    char fname[MAXPATHSIZE];
    char nameDirFile[MAXPATHSIZE];
    
    char PathFifo[MAXPATHSIZE]; //fifo olusturmak icin tutulan path
    char StringUnlink[MAXPATHSIZE]; //unlink etmek icin bilgilerin string olarak tutuldugu gecici array
    char path[MAXPATHSIZE]; //icinde bulunulan path
    char tempPath[MAXPATHSIZE]; //icinde bulunulan pathi olusturmak icin kullanilan temp path
    char **PathUnlink = NULL; //unlink etmek icin tum bilgilerin tutuldugu arra
    int iSizeUnlink = 0; //unlink etmek icin tum bilgilerin tutuldugu arrayin size'i
    
    int **arrPipe; //pipe array
    int iSizePipe = 0;
    int *arrFifo; //fifo array
    int iSizeFifo = 0;
    
    int fd; //checkStringFile'a gonderilecek file descriptor
    
    int iCount = 0; //bir file daki string sayisini tutar
    int iWords = 0; //toplam word sayisi
    int i = 0;
    int size = 0;
    int iThreads = 0; //olusturulan thread sayisini hesaplamak icin tutulan degisken
    int iTh = 0;
    char s[MAXSIZE]; //fifoya(loga) yazmak icin kullanilan string
    
    pthread_t myThread;
    ////////////////////////////////////////////////
    
    //fifo ve pipe arrayleri icin yer ayrildi
    arrFifo = malloc(0);
    arrPipe = malloc(0);

    getcwd(tempPath, MAXPATHSIZE);
    sprintf(path, "%s/%s", tempPath, sNameDir);
    
    chdir(path); 
    //path bufa atilip kontrol edildi
    if (stat(path, &buf) == -1)
        return -1;
    // directory testi yapildi
    if (S_ISDIR(buf.st_mode)) {
        if ((dir = opendir(path)) == NULL) { //directory acilmiyorsa cikis yapildi
            printf("%s directory can't open.\n", path);
            printf("  Exit Condition: (cant open directory) due to error %d\n", EXIT_FAILURE);
            sprintf(s, "  Exit Condition: (cant open directory) due to error %d\n", EXIT_FAILURE);
            write(fifo2, s, strlen(s)); 
            exit(EXIT_FAILURE);
        }
        else {
            //butun directoryleri dolasip file larda stringi arandi
            while ((mdirent = readdir(dir)) != NULL) {
                if (strcmp(mdirent->d_name, ".") != 0 && strcmp(mdirent->d_name, "..") != 0 && mdirent->d_name[strlen(mdirent->d_name) - 1] != '~') {
                    sprintf(nameDirFile, "%s/%s", path, mdirent->d_name);
                    if (stat(nameDirFile, &buf) == -1){
                        printf("  Exit Condition: (stat(dir)) due to error %d\n", EXIT_FAILURE);
                        sprintf(s, "  Exit Condition: (stat(dir)) due to error %d\n", EXIT_FAILURE);
                        write(fifo2, s, strlen(s)); 
                        return -1;
                    }
                    //directoryse fifo olusturup fork yaptim, directory'ler fifo ile haberlesecegi icin
                    if (S_ISDIR(buf.st_mode)) {
                        iSizeFifo++;
                        iDirs += iSizeFifo; //directory sayisi hesaplandi
                        
                        arrFifo = realloc(arrFifo, iSizeFifo * sizeof (int));

                        strcpy(PathFifo, nameDirFile);
                        sprintf(PathFifo, "%d", iSizeFifo);
                        mkfifo(PathFifo, (S_IRUSR | S_IWUSR)); //fifoyu olusturdum
                      
                        sprintf(StringUnlink, "%s/%d" ,  getcwd(StringUnlink, MAXPATHSIZE), iSizeFifo);    

                        //unlink etmek icin path bilgisi tutuldu
                       	PathUnlink = (char **) realloc(PathUnlink, (iSizeUnlink + 1) * sizeof(*PathUnlink));
                        PathUnlink[iSizeUnlink] = (char *)malloc(2 * sizeof(StringUnlink));
                        iSizeUnlink++;
                        sprintf(PathUnlink[size], "%s/%d" ,  getcwd(PathUnlink[size], MAXPATHSIZE), iSizeFifo);                  
                        size++;

                        pid = fork(); //fork olusturuldu
                        if(pid == -1){ //fork olusturulamadiysa cikis yapildi
                             perror("Failed to fork\n");
                             printf("  Exit Condition: (cant fork) due to error %d\n", EXIT_FAILURE);
                             sprintf(s, "  Exit Condition: (cant fork) due to error %d\n", EXIT_FAILURE);
                             write(fifo2, s, strlen(s));   
                             closedir(dir);
                             return 0;
                        }
                        if (pid == 0) { //child verileri pipe yazdi
                            while (((fd = open(PathFifo, O_WRONLY)) == -1) && (errno == EINTR));
                            if (fd == -1) {
                                fprintf(stderr, "[%ld]:failed to open named pipe %s for write: %s\n",
                                        (long) getpid(), PathFifo, strerror(errno));
                                printf("  Exit Condition: (cant open pipe) due to error %d\n", EXIT_FAILURE);
                                sprintf(s, "  Exit Condition: (cant open pipe) due to error %d\n", EXIT_FAILURE);
                                write(fifo2, s, strlen(s));
                                return 1;
                            }
                            //child directory icindeki ya da yanindaki bir path(directory OR file) ile tekrar DirWalk'i cagirir
                            iWords += DirWalk(mdirent -> d_name, fd, semp);
                            closedir(dir);
                            close(fd);
				            //fork yapildiginda olusan process parant'in sahip oldugu her seye sahip olur
		                    free(sSearchStr); //isi biten processte kopyasi olusan bu string bosaltimak zorunda
                            exit(iWords);
                        } else { //parenta degerler verildi
                            while (((arrFifo[iSizeFifo - 1] = open(PathFifo, O_RDONLY)) == -1) && (errno == EINTR));
                            if (arrFifo[iSizeFifo - 1] == -1) {
                                fprintf(stderr, "[%ld]:failed to open named pipe %s for write: %s\n",
                                        (long) getpid(), PathFifo, strerror(errno));
                                printf("  Exit Condition: (cant open pipe) due to error %d\n", EXIT_FAILURE);
                                sprintf(s, "  Exit Condition: (cant open pipe) due to error %d\n", EXIT_FAILURE);
                                write(fifo2, s, strlen(s));
                                return 1;
                            }
                        }
                    }      
                    //eger file ise pipe olusturup fork yaptim
                    else if (S_ISREG(buf.st_mode)) {                                                
                        iSizePipe++;
                        arrPipe = realloc(arrPipe, iSizePipe * sizeof (int*));
                        arrPipe[iSizePipe - 1] = malloc(sizeof (int) * 2);
                        pipe(arrPipe[iSizePipe - 1]);
              
                        //fname'e arastirilacak dosya adi yazildi
                        sprintf(fname, "%s", mdirent->d_name);
                        //arastirilacak dosya adi thread'e yollanilacak structure yapisina assign edildi
                        sprintf(dataOfThread[sizeOfThread].fname, "%s", fname);
                        //thread'e yollanilacak structure yapisina pipe assign edildi, bu pipe gerekli deger yazilacak
                        dataOfThread[sizeOfThread].pipeData = arrPipe[iSizePipe - 1][1];
                        //arastirma isleminin gerceklestirdigi process'in IDsi bulundu
                        dataOfThread[sizeOfThread].processID = getppid();
                 
                        iThreads++;
                        //fname icin thread olusturuldu
                        pthread_create(&threadArr[sizeOfThread], NULL, threadOperations, (void *)&dataOfThread[sizeOfThread]);
                        iTh++;
                        //printf("iTh:%d\n", iTh);
                        ++sizeOfThread;
                    }
                }
            }
        }

        while (r_wait(NULL) > 0); //parent child lari bekliyor

        //threadlarin islemlerini bitirip gelmesi beklendi
        for (i = 0; i < sizeOfThread; ++i)
            pthread_join(threadArr[i], NULL);
           
        //thread sayisi temp dosyasina yazildi 
        fprintf(fPtrThreads, "%d\n", iThreads);
        
        if(iTh >= iMaxThreads){
            iMaxThreads = iTh;
            iTh = 0;
            //printf("max:%d\n", iMaxThreads);
        }
          
        //semaphore isleminin bitmesi beklendi  
        sem_wait(semp);

        //pipe'lar degerleri fifoya verildi
        for (i = 0; i < iSizePipe; ++i) {
            close(arrPipe[i][1]);
            copyfile(arrPipe[i][0], fifo);
            close(arrPipe[i][0]);
        }

        //semaphore islemi bitirildi.
        sem_post(semp);

        //olusan fifolar temizlendi
 		for (i = 0; i < size; ++i) {
            copyfile(arrFifo[i], fifo);
            close(arrFifo[i]);
            unlink(PathUnlink[i]);
        }
        //unlink bilgilerinin tutuldugu array bosaltildi
        for (i = 0; i < size; i++)
            free(PathUnlink[i]);
        free(PathUnlink);

        //pipelar temizlendi
        for (i = 0; i < iSizePipe; ++i)
            free(arrPipe[i]);
        free(arrPipe);
        free(arrFifo);  
    }
    closedir(dir);
    
    //Her sinyalin gelme olasiligi oldugu icin her sinyal icin yakalama islemi gerceklestirildi
    signal(SIGINT, signalHandle);
    signal(SIGTERM, signalHandle);
    signal(SIGILL, signalHandle);
    signal(SIGABRT, signalHandle);
    signal(SIGFPE, signalHandle);
    signal(SIGSEGV, signalHandle);
    
    return iWords;
}

/**
 * thread kullanimi icin yazildi
 * her olusturulan thread icin bu fonksiyon calsitirilir.
 * threadler her file icin olusturulur, boylece her file daki string sayisi bulunur.
 *
 * dataOfThread: 
 *          thread icin gelen parametre.
 *          thread icinde kullanilan fonksiyonun parametreleri!
 */
void *threadOperations(void * dataOfThread){  
    threadData *currentThreadData;
    currentThreadData = (threadData *) dataOfThread;

    currentThreadData->threadID = getpid();

    iNumOfLine = 0;
    // dosyadaki string sayisi bulunur ve temp dosyasina yazilir.
    searchStringInFile(currentThreadData->fname, currentThreadData->pipeData, currentThreadData->processID, currentThreadData->threadID);

    fprintf(fPtrLines, "%d\n", iNumOfLine);
    //printf("%d\n", iNumOfLine); 

    int iFiles=0;
    iFiles++;
    fprintf(fPtrFiles, "%d\n", iFiles);

    // thread'in islemi bittigi icin thread sonlandirilir.
    pthread_exit(NULL);
}


/**
 * Yapilan islemler main de kafa karistirmasin diye hepsini bu fonksiyonda 
 * topladim.
 *
 * sFileName: 
 * String, input, icinde arama yapilacak dosyanin adini tutuyor
 */
int searchStringInFile(char* sFileName, int pipeFd, pid_t pID, pid_t tID){
    char **sStr=NULL;
    int i=0, j=0;
    int iCount = 0;
    char s[MAXSIZE];
    //Burada adi verilen dosyanin acilip acilmadigina baktim
    //Acilamadiysa programi sonlandirdim.
    fPtrInFile = fopen (sFileName, "r");
    if (fPtrInFile == NULL) {
        perror (sFileName);
        fprintf(stderr, "  Exit Condition: due to error 1\n");
		sprintf(s, "  Exit Condition: due to error 1\n");
        write(fifo2, s, strlen(s));
        exit(1);
    }

    if(isEmpty(fPtrInFile) == 1){
        rewind(fPtrInFile);
        //Dosyanin satir sayisini ve en uzun satirin 
        //column sayisini bulan fonksiyonu cagirdim.
        findLengthLineAndNumOFline();
        
        //Dosyayi tekrar kapatip acmak yerine dosyanin nerede oldugunu 
        //gosteren pointeri dosyanin basina aldim
        rewind(fPtrInFile);

        //Dosyayi string arrayine okudum ve bu string'i return ettim
        sStr=readToFile();

        #ifndef DEBUG //Dosyayi dogru okuyup okumadigimin kontrolü
            printf("File>>>>>>>\n");
            for(i=0; i<iNumOfLine; i++)
                printf("%s\n", sStr[i]);
        #endif
        
        //s = (char*)calloc(MAXSIZE, sizeof(char));
        
        //String arrayi icinde stringi aradim ve sayisini iCount'a yazdim
        iCount=searchString(sFileName, sStr, pipeFd, pID, tID);
        //iWordCount += iCount;
        sprintf(s, "\n****************************************************\n");
        write(pipeFd, s, strlen(s));
        sprintf(s,"%s found %d in total in %s file\n", sSearchStr, iCount, sFileName); 
        write(pipeFd, s, strlen(s));
        sprintf(s, "****************************************************\n\n");
        write(pipeFd, s, strlen(s));
        
        //free(s);
        
        //Strin icin ayirdigim yeri bosalttim
        for(i=0; i<iNumOfLine; i++)
            free(sStr[i]);
        free(sStr);
    }
    fclose(fPtrInFile);
    
    fprintf(fPtrWords, "%d\n", iCount);
    
    return iCount;
}


/**
 * String arama isleminin ve her yeni bir string bulundugunda bulunan 
 * kelime sayisinin arttirildigi fonksiyon
 *
 * sFile     :String arrayi, input, ıcınde arama yapiacak string arrayi
 * sFileName :String'in aranacagi dosya adi
 * return degeri ise integer ve bulunan string sayisini return eder
 */
int searchString(char* sFileName, char **sFile, int pipeFd, pid_t pID, pid_t tID){
    int i=0, j=0;
    int iRow=0, iCol=0;
    char *word=NULL;
    int iCount=0;
    char s[MAXSIZE];    
    //string arrayinin her satirini sira ile str stringine kopyalayip inceleyecegim
    word=(char *)calloc(100, sizeof(char));
    //s = (char *)calloc(MAXSIZE, sizeof(char));
    for(i=0; i<iNumOfLine; i++){ //Satir sayisina gore donen dongu
        for(j=0; j<iLengthLine; j++){ //Sutun sayisina gore donen dongu
                //printf("i:%d\tj:%d\n", i, j);
            if((copyStr(sFile, word, i, j, &iRow, &iCol)) == 1){ //str stringine kopyalama yaptim
                //kopyalama ile sSearchStr esit mi diye baktim
                if(strncmp(word, sSearchStr, (int)strlen(sSearchStr)) == 0){
                    #ifndef DEBUG
                        printf("%d-%d %s: [%d, %d] %s first character is found.\n",pID, tID, sFileName, iRow, iCol, sSearchStr);
                    #endif
                	//Bulunan kelimenin satir ve sutun sayisi LogFile'a yazdim
                	sprintf(s, "%d-%d %s: [%d, %d] %s first character is found.\n", pID, tID, sFileName, iRow, iCol, sSearchStr);
                	write(pipeFd, s, strlen(s));
                    iCount++; //String sayisini bir arttirdim kelime buldugum icin
                }
            }
        }
    }
    //free(s);
    free(word);
    return iCount; //Bulunan string sayisini return ettim
}

/**
 * Aranmasi gereken stringin karakter sayisi kadar karakteri word stringine kopyalar.
 * Kopyalama yaparken kopyalanan karakterin space(' '), enter('\n') ve tab('\t') 
 * olmamasina dikkat ederek kopyalar.
 * 
 * sFile    :Dosyadaki karakterlerin tutuldugu iki boyutlu karakter arrayi
 * word     :Kopyalanan karakterlerin tutulacagi 1 karakter arrayi
 * iStartRow:Aramanin baslayacagi satir indexi
 * iStartCol:Aramanin baslayacagi sutun indexi
 * iRow     :Bulunan kelimenin ilk karakterinin bulundugu satir numarasi
 * iCol     :Bulunan kelimenin ilk karakterinin bulundugu sutun numarasi
 */
int copyStr(char **sFile, char* word, int iStartRow, int iStartCol, int *iRow, int *iCol){

    int k=0, i=0, j=0, jStart = 0;
    //printf("iStartRowIndex:%d\tiStartColIndex:%d\n", iStartRow, iStartCol);
    
    if(sFile[iStartRow][iStartCol] == '\n' || sFile[iStartRow][iStartCol] == '\t' || sFile[iStartRow][iStartCol] == ' '){
        return 0;
    }  
    else{
        *iRow = iStartRow+1;
        *iCol = iStartCol+1;
	    #ifndef DEBUG
    	    printf("iRow:%d\tiCol:%d\n", *iRow, *iCol);
		    printf("iStartRow:%d\tiStartCol:%d\n", iStartRow, iStartCol);
	    #endif
        k=0;
        jStart = *iCol-1;
        for(i=*iRow-1; i<iNumOfLine && k < iSizeOfSearchStr; i++){
            for(j=jStart; j<iLengthLine && k < iSizeOfSearchStr; j++){
		        if(sFile[i][j] != '\n' && sFile[i][j] != '\t' && sFile[i][j] != ' ' && k < iSizeOfSearchStr){
                    word[k] = sFile[i][j];
                    k++;
                }                
		        if(sFile[i][j] == '\n' && k < iSizeOfSearchStr){
                    j=iLengthLine;
                }
            }
	        jStart=0; //jnin bir alt satirda baslangic konumu 0 olarak ayarlandi
        }    
        if(k != iSizeOfSearchStr)
            return 0;
        else
            return 1;
    }
    return -1;
}



/**
 * dosyanin bos olup olmadigina bakar
 * 1->bos degil
 * 0->bos
 */
int isEmpty(FILE *file){
    long savedOffset = ftell(file);
    fseek(file, 0, SEEK_END);

    if (ftell(file) == 0){
        return 0;
    }

    fseek(file, savedOffset, SEEK_SET);
    return 1;
}


/**
 * Dosyadaki satir sayisini ve en uzun sutundaki karakter sayisini bulur.
 * Burdan gelen sonuclara gore dynamic allocation yapilir.
 */
void findLengthLineAndNumOFline(){
	int iLenghtLine=0;
	int iMaxSize=0;
	char ch=' ';

		while(!feof(fPtrInFile)){
			fscanf(fPtrInFile, "%c", &ch);
			iMaxSize++;
				if(ch == '\n'){
					iNumOfLine=iNumOfLine+1;
					if(iMaxSize >=(iLengthLine))
						iLengthLine=iMaxSize;
					iMaxSize=0;
				}
		}
		iNumOfLine-=1; //bir azalttim cunku dongu bir defa fazla donuyor ve iNumOfLine
                        //bir fazla bulunuyor.
        iLengthLine+=1;
        #ifndef DEBUG
            printf("iLengthLine:%d\tiNumOfLine:%d\n", iLengthLine, iNumOfLine);
        #endif
}

/**
 * Dosya okunur ve iki boyutlu bir karakter arrayyine atilir.
 * Karakter arrayi return edilir.    
 *
 * return : char**, okunan dosyayi iki boyutlu stringe aktardim ve 
 *          bu string arrayini return ettim.
 */
char** readToFile(){
    char **sFile=NULL;
    int i=0;
    char s[MAXSIZE];

    //Ikı boyutlu string(string array'i) icin yer ayirdim
    sFile=(char **)calloc(iNumOfLine*iLengthLine, sizeof(char*));
    if( sFile == NULL ){ //Yer yoksa hata verdim
        #ifndef DEBUG
            printf("INSUFFICIENT MEMORY!!!\n");
        #endif
        fprintf(stderr, "  Exit Condition: (INSUFFICIENT MEMORY) due to error 1\n");
		sprintf(s, "  Exit Condition: (INSUFFICIENT MEMORY) due to error 1\n");
        write(fifo2, s, strlen(s));
        exit(1);
    }
    //Ikı boyutlu oldugu ıcın her satir icinde yer ayirdim
    for(i=0; i<iNumOfLine; i++){
        sFile[i]=(char *)calloc(iLengthLine, sizeof(char));
        if( sFile[i] == NULL ){ //Yer yoksa hata verdim
            #ifndef DEBUG
                printf("INSUFFICIENT MEMORY!!!\n");
            #endif
            fprintf(stderr, "  Exit Condition: (INSUFFICIENT MEMORY) due to error 1\n");
	        sprintf(s, "  Exit Condition: (INSUFFICIENT MEMORY) due to error 1\n");
            write(fifo2, s, strlen(s));
            exit(1);
        }
    }

    i=0;
    do{ //Dosyayi okuyup string arrayine yazdim
    
        fgets(sFile[i], iLengthLine, fPtrInFile);
        #ifndef DEBUG
            printf("*-%s-*\n", sFile[i]);
        #endif
        i++;
    }while(!feof(fPtrInFile));

    return sFile;
}

/**
 * structure yapisindaki degerler initialize edildi.
 * temp dosyalari acildi
 * temp dosyalarinin pathleri tutuldu
 */
void openFiles(char path[MAXPATHSIZE]){
    numOfValues.iNumOfWords = 0;
    numOfValues.iNumOfDirectories = 1;
    numOfValues.iNumOfFiles = 0;
    numOfValues.iNumOfLines = 0;
    numOfValues.iNumOfProcess = 0;
    numOfValues.iNumOfThreads = 0;
    numOfValues.iTimeOperations = 0.0;
    
    
    fPtrWords = fopen("words.txt", "w+"); //file'lardaki string sayisinin yazildigi file
    fPtrFiles = fopen("files.txt", "w+");
    fPtrLines = fopen("lines.txt", "w+");
    fPtrThreads = fopen("threads.txt", "w+");
    
    
    sprintf(pathWords, "%s/words.txt", path);
    sprintf(pathFiles, "%s/files.txt", path);
    sprintf(pathLines, "%s/lines.txt", path);
    sprintf(pathThreads, "%s/threads.txt", path);
}

/**
 * acilan temp dosyalar kapatildi.
 * temp dosyalar kaldirildi
 */ 
void closeFiles(){
    fclose(fPtrWords);
    fclose(fPtrFiles);
    fclose(fPtrLines);
    fclose(fPtrThreads);
    
    remove(pathWords);
    remove(pathFiles);
    remove(pathLines);
    remove(pathThreads);
}

/**
 * gereken degerler hesaplandi.
 */
void calculateNumOfValues(){
    numOfValues.iNumOfWords = 0;
    numOfValues.iNumOfDirectories = 1;
    numOfValues.iNumOfFiles = 0;
    numOfValues.iNumOfLines = 0;
    numOfValues.iNumOfProcess = 0;
    numOfValues.iNumOfThreads = 0;
    
    //string sayisi hesaplandi
    int i = 0;
    rewind(fPtrWords);
    while(!feof(fPtrWords)){
        fscanf(fPtrWords, "%d", &i);
        numOfValues.iNumOfWords += i;
    }
    numOfValues.iNumOfWords -= i; //1 tane fazladan okuyor
    
    //directory sayisi hesaplandi
    numOfValues.iNumOfDirectories += iDirs;
    
    //file sayisi hesaplandi
    i = 0;
    rewind(fPtrFiles);
    while(!feof(fPtrFiles)){
        fscanf(fPtrFiles, "%d", &i);
        numOfValues.iNumOfFiles += i;
    }
    numOfValues.iNumOfFiles -= i; //1 tane fazladan okuyor
    
    //okunan satir sayisi hesaplandi
    i = 0;
    rewind(fPtrLines);
    while(!feof(fPtrLines)){
        fscanf(fPtrLines, "%d", &i);
        numOfValues.iNumOfLines += i;
    }
    numOfValues.iNumOfLines -= i; //1 tane fazladan okuyor

    //olusturulan process sayisi hesaplandi
    numOfValues.iNumOfProcess = numOfValues.iNumOfDirectories;
    
    numOfValues.iNumOfCascadeThreads = numOfValues.iNumOfDirectories - 1;
    //olusturulan toplam thread sayisi hesaplandi
    i = 0;
    rewind(fPtrThreads);
    while(!feof(fPtrThreads)){
        fscanf(fPtrThreads, "%d", &i);
        numOfValues.iNumOfThreads += i;
    }
    numOfValues.iNumOfThreads -= i; //1 tane fazladan okuyor   
    
    numOfValues.iNumOfMaxThreads = iMaxThreads; 
}

/**
 * Gelen sinyal yakalanir 
 * Gerekli cikis islemi gerceklestirilir
 * Hangi sinyal nedeni ile cikildigi yazilir
 */
void signalHandle(int sig){
    char s[MAXSIZE];
    printf("  Exit Condition: due to signal no:%d\n", sig);
    printf("*******************************************************\n\n");
    write(fifo2, s, strlen(s)); 
    sprintf(s, "  Exit Condition: due to signal no:%d\n", sig);
    write(fifo2, s, strlen(s)); 
    sprintf(s, "*******************************************************\n\n");
    write(fifo2, s, strlen(s));
    closeFiles();
    exit(1);
}

///////////////////////////////////////////////////////////////////////////////
