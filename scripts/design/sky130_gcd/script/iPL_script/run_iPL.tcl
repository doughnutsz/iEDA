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
def_init -path $::env(RESULT_DIR)/iTO_fix_fanout_result.def

#===========================================================
##   run Placer
#===========================================================
run_placer -config $::env(CONFIG_DIR)/pl_default_config.json

#===========================================================
##   save def 
#===========================================================
def_save -path $::env(RESULT_DIR)/iPL_result.def

#===========================================================
##   save netlist 
#===========================================================
netlist_save -path $::env(RESULT_DIR)/iPL_result.v -exclude_cell_names {}

#===========================================================
##   report 
#===========================================================
report_db -path "$::env(RESULT_DIR)/report/pl_db.rpt"

report_wirelength -path  "$::env(RESULT_DIR)/report/eval/iPL_result_wirelength0.rpt"
report_congestion -path "$::env(RESULT_DIR)/report/eval/iPL_result_congestion0.rpt"

#===========================================================
##   Exit 
#===========================================================
flow_exit
