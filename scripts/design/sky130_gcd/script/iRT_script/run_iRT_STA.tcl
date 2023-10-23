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
def_init -path ./result/iRT_result.def

#===========================================================
##   run STA
#===========================================================

init_rt -temp_directory_path "./result/rt/" \
        -bottom_routing_layer "met1" \
        -top_routing_layer "met5"

run_rt

init_sta -output ./result/sta/timing.log
report_timing -stage "dr"

destroy_rt

#===========================================================
##   Exit 
#===========================================================
flow_exit
