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
##   read lef
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

#===========================================================
##   read def
#===========================================================
def_init -path $::env(RESULT_DIR)/iRT_result.def

#===========================================================
##   save def & verilog
#===========================================================
def_save -path $::env(RESULT_DIR)/data_out.def
netlist_save -path $::env(RESULT_DIR)/data_out.v

#===========================================================
##   Exit 
#===========================================================
flow_exit

