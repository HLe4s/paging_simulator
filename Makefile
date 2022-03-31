OBJ_FILES=paging_simulator

all : paging_simulator.c
	gcc -g -o $(OBJ_FILES) $<

clean : 
	rm -rf $(OBJ_FILES)
