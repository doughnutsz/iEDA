#===========================================================
##   init flow config
#===========================================================
flow_init -config $::env(CONFIG_DIR)/flow_config.json

#===========================================================
##   read db config
#===========================================================
db_init -config $::env(CONFIG_DIR)/db_default_config.json

#===========================================================
##   reset data path
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl

#===========================================================
##   reset lib
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lib.tcl

#===========================================================
##   reset sdc
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_sdc.tcl

#===========================================================
##   read lef
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

#===========================================================
##   read def
#===========================================================
def_init -path $::env(RESULT_DIR)/iPL_lg_result.def

#===========================================================
##   run Router
#===========================================================
init_rt -temp_directory_path "$::env(RESULT_DIR)/rt/" \
        -bottom_routing_layer "met1" \
        -top_routing_layer "met4" \
        -thread_number 64

run_rt

# init_sta -output $::env(RESULT_DIR)/rt/sta/
# report_timing -stage "dr"

destroy_rt

report_wirelength -path  "$::env(RESULT_DIR)/report/eval/iRT_result_wirelength0.rpt"

#===========================================================
##   save def & netlist
#===========================================================
def_save -path $::env(RESULT_DIR)/iRT_result.def

#===========================================================
##   save netlist 
#===========================================================
netlist_save -path $::env(RESULT_DIR)/iRT_result.v -exclude_cell_names {}

#===========================================================
##   report db summary
#===========================================================
report_db -path "$::env(RESULT_DIR)/report/rt_db.rpt"

#===========================================================
##   Exit 
#===========================================================
flow_exit
