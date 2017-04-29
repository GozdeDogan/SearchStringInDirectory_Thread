all: 
	  clear
	  gcc 131044019_Gozde_Dogan_HW4.c restart.c -o grepTh -lpthread
clean:
	  rm *.o grepTh
