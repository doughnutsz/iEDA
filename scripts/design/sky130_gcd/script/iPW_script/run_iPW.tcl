#===========================================================
##   init flow config
#===========================================================
flow_init -config ./iEDA_config/flow_config.json

#===========================================================
##   read db config
#===========================================================
db_init -config ./iEDA_config/db_default_config.json

#===========================================================
##   reset data path
#===========================================================
source ./script/DB_script/db_path_setting.tcl

#===========================================================
##   reset lib
#===========================================================
source ./script/DB_script/db_init_lib.tcl

#===========================================================
##   reset sdc
#===========================================================
source ./script/DB_script/db_init_sdc.tcl

#===========================================================
##   read lef
#===========================================================
source ./script/DB_script/db_init_lef.tcl

#===========================================================
##   read def
#===========================================================
def_init -path ./result/iPL_result.def

#===========================================================
##   run STA
#===========================================================
#set_design_workspace "./result/sta/"
run_power -output ./result/sta/

#===========================================================
##   Exit 
#===========================================================
flow_exit
