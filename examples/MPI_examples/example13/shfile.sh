make
export SIGHT_FILE_OUT=1
mpirun -np 4 ex
export SIGHT_FILE_OUT=1
export SIGHT_MERGE_SEPARATE=1
/g/g92/polyzou1/pro_sight/sight/hier_merge dbgOut zipper dbg.rank_*/structure

/g/g92/polyzou1/pro_sight/sight/slayout dbgOut/structure;

#copy files out
#rm -rf ../htmlfiles/example13
#mkdir ../htmlfiles/example13
#cp dbgOut/index.html ../htmlfiles/example13/   
#cp -r dbgOut/html ../htmlfiles/example13/