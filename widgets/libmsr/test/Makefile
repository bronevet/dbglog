include ../../../Makefile

all: libmsr_test libmsr_mpi_test libmsr_powercap_test


libmsr_test: libmsr_test.c 
	${CC} -Wall -o libmsr_test libmsr_test.c -L ../lib -lmsr

libmsr_mpi_test: libmsr_test.c 
	mpicc -Wall -DMPI -o libmsr_mpi_test libmsr_test.c -L ../lib -lmsr

libmsr_powercap_test: libmsr_powercap_test.c 
	${CC} -Wall -o libmsr_powercap_test libmsr_powercap_test.c -L ../lib -lmsr

mpi: mpi_test.c
	mpicc -Wall -o mpi_test mpi_test.c -L../lib -lgaramond 

rzoz: 
	srun -p quad_sb -n 32 --auto-affinity=start=0,verbose,cpt=1 ./mpi_test | grep QQQ > QQQ

clean:
	rm -f mpi_test libmsr_test


