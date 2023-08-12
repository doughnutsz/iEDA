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
##   read lef
#===========================================================
source ./script/DB_script/db_init_lef.tcl

#===========================================================
##   read def
#===========================================================
def_init -path ./result/iTO_hold_result.def

#===========================================================
##   run Placer
#===========================================================
run_incremental_flow -config ./iEDA_config/pl_default_config.json

#===========================================================
##   save def 
#===========================================================
def_save -path ./result/iPL_lg_result.def

#===========================================================
##   save netlist 
#===========================================================
netlist_save -path ./result/iPL_lg_result.v -exclude_cell_names {}

#===========================================================
##   report db summary
#===========================================================
report_db -path "./result/report/lg_db.rpt"

#===========================================================
##   Exit 
#===========================================================
flow_exit