#include "SSPIEm.h"

/************************************************************************
* Lattice Semiconductor Corp. Copyright 2011
*
* SSPI Embedded System
*
* Version: 4.0.0
*		Add SSPIEm_preset() function prototype 
*
*
* Main processing engine
*
*************************************************************************/
#include "core.h"
#include "intrface.h"

#include <linux/slab.h>


/*********************************************************************
*
* SSPIEm_preset()
*
* This function calls algoPreset and dataPreset to set which algorithm
* and data files are going to be processed.  Input(s) to the function
* may depend on configuration.
*
*********************************************************************/
int SSPIEm_preset(unsigned char *setAlgoPtr, unsigned int setAlgoSize,
				  unsigned char *setDataPtr, unsigned int setDataSize){
	int retVal = algoPreset(setAlgoPtr, setAlgoSize);
	if(retVal)
		retVal = dataPreset(setDataPtr, setDataSize);
	return retVal;
}
/************************************************************************
* Function SSPIEm
* The main function of the processing engine.  During regular time,
* it automatically gets byte from external storage.  However, this 
* function requires an array of buffered algorithm during 
* loop / repeat operations.  Input bufAlgo must be 0 to indicate
* regular operation.
*
* To call the VME, simply call SSPIEm(int debug);
*************************************************************************/
int SSPIEm(unsigned int algoID){
	int retVal = 0;
	retVal = SSPIEm_init(algoID);
//	pr_info("#1 retVal = %d\n", retVal);
	if(retVal <= 0)
		return retVal;
	retVal = SSPIEm_process(0,0);
//	pr_info("#2 retVal = %d\n", retVal);
	return retVal;
}

