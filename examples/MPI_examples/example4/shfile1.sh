rm -rf dbg*
make
export SIGHT_FILE_OUT=1
mpirun -np 4 ex
export SIGHT_FILE_OUT=1
#/g/g92/polyzou1/pro_sight/sight/hier_merge dbgOut common dbg.rank_*/structure
/g/g92/polyzou1/pro_sight/sight/hier_merge dbgOut1 common dbg.rank_1/structure dbg.rank_2/structure dbg.rank_3/structure
#/g/g92/polyzou1/pro_sight/sight/slayout dbgOut1/structure;

export SIGHT_MERGE_SEPARATE=1
/g/g92/polyzou1/pro_sight/sight/hier_merge dbgOut zipper  dbg.rank_0/structure dbgOut1/structure
#dbg.rank_0/structure dbgOut1/structure 
# dbg.rank_*/structure


/g/g92/polyzou1/pro_sight/sight/slayout dbgOut/structure;

#copy files out
#rm -rf ../htmlfiles/example5
#mkdir ../htmlfiles/example5
#cp dbgOut/index.html ../htmlfiles/example5/   
#cp -r dbgOut/html ../htmlfiles/example5/


#rm -r ../htmlfiles/*
#cp dbgOut/index.html ../htmlfiles -r
#cp dbgOut/html ../htmlfiles -r
#tar -cvf ../file.tar ../htmlfiles/