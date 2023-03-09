#include "DSA_controller.h"

/*****
 * Initialize most basic variables and objects here. Most of the setup should
 * be done in the Init(...) function instead of here where possible.
 *****/
DSA_controller::DSA_controller():
    NumberOfRobots(0),
    NumberOfSpirals(0),
    DSA(START),
    RNG(NULL),
    ResetReturnPosition(true),
    stopTimeStep(0),
    m_pcLEDs(NULL),
    isHoldingFood(false),
	num_targets_per_min(0)
	{}

/*****
 * Initialize the controller via the XML configuration file. ARGoS typically
 * wants objects & variables initialized here instead of in the constructor(s).
 *****/
void DSA_controller::Init(TConfigurationNode& node) {

    compassSensor   = GetSensor<argos::CCI_PositioningSensor>("positioning");
    wheelActuator   = GetActuator<argos::CCI_DifferentialSteeringActuator>("differential_steering");
    proximitySensor = GetSensor<argos::CCI_FootBotProximitySensor>("footbot_proximity");

    argos::TConfigurationNode settings = argos::GetNode(node, "settings");
    argos::GetNodeAttribute(settings, "SearchStepSize",          SearchStepSize);
    argos::GetNodeAttribute(settings, "NestDistanceTolerance", NestDistanceTolerance);
    argos::GetNodeAttribute(settings, "NestAngleTolerance", NestAngleTolerance);
    argos::GetNodeAttribute(settings, "TargetDistanceTolerance", TargetDistanceTolerance);
    argos::GetNodeAttribute(settings, "TargetAngleTolerance",    TargetAngleTolerance);
    argos::GetNodeAttribute(settings, "FoodDistanceTolerance",   FoodDistanceTolerance);
    argos::GetNodeAttribute(settings, "RobotForwardSpeed",       RobotForwardSpeed);
    argos::GetNodeAttribute(settings, "RobotRotationSpeed",      RobotRotationSpeed);
    argos::GetNodeAttribute(settings, "ResultsDirectoryPath",      results_path);
    argos::GetNodeAttribute(settings, "DestinationNoiseStdev",      DestinationNoiseStdev);
    argos::GetNodeAttribute(settings, "PositionNoiseStdev",      PositionNoiseStdev);
    argos::GetNodeAttribute(settings, "ProbTargetDetection",      ProbTargetDetection);
    //FoodDistanceTolerance *= FoodDistanceTolerance;
    SquaredFoodDistanceTolerance = FoodDistanceTolerance * FoodDistanceTolerance; //squared distance
    
   // m_pcLEDs   = GetActuator<CCI_LEDsActuator>("leds");
	//	m_pcLEDs->SetAllColors(CColor::GREEN);
		controllerID= GetId();//qilu 07/26/2016

    RNG = CRandom::CreateRNG("argos");
    
     string ID = GetId();
    string ID_number;
    //LOG<<"Robot ID string= "<<ID<<endl;
      
    for(size_t i=1; i< ID.size(); i++){
      ID_number += ID[i];
    }
    RobotID = stoi(ID_number);
 
    //LOG<<"RobotID number="<<RobotID<<endl;
    //SetStartPosition(argos::CVector3(regionCenters[RobotID].GetX(), regionCenters[RobotID].GetY(), 0.0));
    TrailColor = CColor(std::rand()%100, std::rand()%150, std::rand()%200, 255); // we avoid the white or nearly white, so we do not mode the random number by 255 

    // Name the results file with the current time and date
 time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    stringstream ss;

    ss << "DSA-"<<GIT_BRANCH<<"-"<<GIT_COMMIT_HASH<<"-" 
       << (now->tm_year) << '-'
       << (now->tm_mon + 1) << '-'
       <<  now->tm_mday << '-'
       <<  now->tm_hour << '-'
       <<  now->tm_min << '-'
       <<  now->tm_sec << ".csv";

    string results_file_name = ss.str();
   results_full_path = results_path+"/"+results_file_name;        

    // Only the first robot should do this:


    if (GetId().compare("DSA_0") == 0)
      {
  
   ofstream results_output_stream;
 results_output_stream.open(results_full_path, ios::app);
 results_output_stream << "NumberOfRobots, "
		       << "NumberOfSpirals, "
		       << "TargetDistanceTolerance, "
		       << "TargetAngleTolerance, "
		       << "SpiralGap, "
		       << "FoodDistanceTolerance, "
		       << "RobotForwardSpeed, "
		       << "RobotRotationSpeed, "
		       << "RandomSeed" << endl
           << NumberOfRobots << ", "
		       << NumberOfSpirals << ", "
		       << TargetDistanceTolerance << ", "
		       << TargetAngleTolerance << ", "
		       << SpiralGap << ", "
		       << FoodDistanceTolerance << ", "
		       << RobotForwardSpeed << ", "
		       << RobotRotationSpeed << ", "
		       << CSimulator::GetInstance().GetRandomSeed() << endl;  
 results_output_stream.close();
      }

    cout << "Finished Initializing the DDSA" << endl;
}
	
bool DSA_controller::IsInTheNest() {
	return ((GetPosition() - loopFunctions->NestPosition).SquareLength() < loopFunctions->NestRadiusSquared);
	}


bool DSA_controller::IsInTheRegion() {
	return ((GetPosition() - loopFunctions->regionCenters[RegionID]).SquareLength() < loopFunctions->RegionRadiusSquared);
	}
	
	
void DSA_controller::printPath(vector<char>& path)
{
    cout << path.size() << endl;
    for(int i = 0; i<path.size(); i++)
    { 
        cout << path.at(i) << endl;
    }
}

bool DSA_controller::GetSpiralPath()
{
	int midIdx, num_points;
	//int reg_idx = -1;
	vector<CVector2> tempPath;
	RegionID=-1;
	
	//LOG<<"RobotID "<<RobotID<< " poisition 1"<<endl;
	//remove visited points 
	for(int i=0; i < NumOfRegions; i++)
	{
		//LOG<<"Flag 1: region "<<i<< ", "<<loopFunctions->shareFlag[i]<<", "<< loopFunctions->singleAssignFlag[i]<<endl;
		//if(loopFunctions->shareFlag[i]==false && loopFunctions->singleAssignFlag[i] == false) //the regions are not be assigned.
		if(loopFunctions->singleAssignFlag[i] == false) // if the region was not assigned, assigned it.
		{
		RegionID = i;
		getReversedSpiralPath(RegionID);
		firstAssigned = true;
		secondAssigned = false;
		loopFunctions->singleAssignFlag[RegionID] = true;
		//LOG<<"RobotID "<<RobotID<< " get the path at region "<< RegionID<<endl;
		return true;
		}
	    //LOG<<"RobotID "<<RobotID<< " poisition 2"<<endl;
        num_points = loopFunctions->spiralPoints[i].size();
        tempPath.clear();
        //The robot has reported its spiral target locaion at least once and the spiral path was not shared.
        if(loopFunctions->firstSpiralTarget[i] != loopFunctions->regionCenters[RegionID] && loopFunctions->shareFlag[i] == false) 
        {
			for(int j= num_points-1; j >= 0; j--)
			{
				if(loopFunctions->spiralPoints[i][j] != loopFunctions->firstSpiralTarget[i])
				{
				tempPath.push_back(loopFunctions->spiralPoints[i][j]);
				}
				else
				{
          //      LOG<<"*** spiralPoints["<< i << "][" << j << "]=" <<loopFunctions->spiralPoints[i][j]<<endl;
                break;
				}
			}
			reverse(tempPath.begin(), tempPath.end());
			loopFunctions->spiralPoints[i].clear();
			loopFunctions->spiralPoints[i] = tempPath;
			tempPath.clear();
			
		}
	} //end for loop for removing visited points
		
	for(int i=0; i < loopFunctions->shareFlag.size(); i++)
	{
		if (loopFunctions->shareFlag[i] == false && loopFunctions->spiralPoints[i].size() > 3) // the spiral path was not shared
		{	
			RegionID = i;
            //LOG<<"the spiral path in region "<< RegionID<< " was not shared, 3. Robot ID="<<RobotID<<endl;
			break;
			}
	}
	
	if(RegionID == -1) //does not have paths for sharing
	{
        //LOG<<"1. Robot "<<RobotID<< " goes to idle ..."<<endl;
		return false;
		}
		
	//check the longest spiral path
	for(int i= 0; i < loopFunctions->spiralPoints.size(); i++)
	{
		if (loopFunctions->spiralPoints[i].size() > loopFunctions->spiralPoints[RegionID].size() && loopFunctions->shareFlag[i] == false && loopFunctions->spiralPoints[i].size() > 3) // make sure the number of spiral points is enough for sharing  
		{
			RegionID = i;
			}
	}
	
//	LOG <<"region id = "<<RegionID<<endl;
	if(RegionID == -1) //does not have paths for sharing
	{
        //LOG<<"2. Robot "<<RobotID<< " goes to idle ..."<<endl;
		return false;
		}
	
	//LOG<<"longest spiralPoints[" <<RegionID<<"] size="<<loopFunctions->spiralPoints[RegionID].size()<<endl;
			
		
		//split the remaining spiral path into two
		
		//LOG<<"region id= "<<RegionID<< ", midIdx="<<midIdx<< ", num_points="<< num_points <<endl;
		//LOG<<"4. Robot ID="<<RobotID<<endl;
        robotSpiralPoints.clear();
	
	robotSpiralPoints = loopFunctions->spiralPoints[RegionID];
		//reverse(robotSpiralPoints.begin(), robotSpiralPoints.end());
			
	firstAssigned = false;
		secondAssigned = true;
		loopFunctions->shareFlag[RegionID] = true;
		loopFunctions->shareAssignUpdated[RegionID] = false;
		return true;
        
		
}

void DSA_controller::getReversedSpiralPath(size_t regID)
{
	robotSpiralPoints = loopFunctions->spiralPoints[regID]; //it makes a copy. It does not make a pointer for the same memory
    reverse(robotSpiralPoints.begin(), robotSpiralPoints.end());
    //LOG<<"Robot "<<RobotID <<", spiral length="<<robotSpiralPoints.size()<<endl;
}

void DSA_controller::CopyPatterntoTemp() 
{
    copy(spiral.begin(),spiral.end(),back_inserter(robotSpiralPoints));
    reverse(robotSpiralPoints.begin(),robotSpiralPoints.end());/* Reverses the robotSpiralPoints */
}

/*****
 * Primary control loop for this controller object. This function will execute
 * the DSA logic once per frame.
 *****/
void DSA_controller::ControlStep() 
{
    if (DSA == START)
    {
		//LOG<<"Start ....."<< endl;
        firstAssigned = false;
        secondAssigned = false;
        NumOfRegions = loopFunctions->spiralPoints.size();
		if(GetSpiralPath())
		{
		    DSA = SEARCHING;
		    Stop(); //stop to reach the initial target location and start on the spiral path
		}
		else
		{
			DSA = IDLE;
		//	LOG<<"Robot"<< RobotID << " set to be idle ..."<<endl;
			SetTarget(loopFunctions->regionCenters[PreRegionID]);
			loopFunctions->IdleCount++;
		//	LOG<<"IdleCount = "<< loopFunctions->IdleCount<< endl;
			
		}
      
        //LOG<<"Robot ID ="<< RobotID<< ", path size="<< robotSpiralPoints.size()<<endl;
        //LOG<<"robotSpiralPoints["<<RobotID<<"]="<<endl;
        //for(int i=0; i< robotSpiralPoints.size(); i++)
        //{
        //    LOG<< "[" << robotSpiralPoints[i]<<"]";
        //}
        //LOG<< "" <<endl;   
        //LOG<< "target=" << GetTarget() << endl;
    }

  // To draw paths
  if (DSA == SEARCHING)
    {
      CVector3 position3d(GetPosition().GetX(), GetPosition().GetY(), 0.00);
      CVector3 target3d(previous_position.GetX(), previous_position.GetY(), 0.00);
      CRay3 targetRay(target3d, position3d);
      myTrail.push_back(targetRay);
  
      loopFunctions->TargetRayList.push_back(targetRay);
      loopFunctions->TargetRayColorList.push_back(TrailColor);
    }

  //LOG << myTrail.size() << endl;
  previous_position = GetPosition();

  /* Continue in a sprial */
  if( DSA == SEARCHING )
  {
      SetIsHeadingToNest(false);
     // LOG << "Robot "<< RobotID<< " SEARCHING" << std::endl;
      SetHoldingFood();
      if (IsHoldingFood())
	  {
	    ReturnPosition = GetPosition();
	    ReturnSpiralPosition = GetTarget();
	    DSA = RETURN_TO_REGION;
	    SetIsHeadingToNest(true);
        //SetTarget(loopFunctions->NestPosition);
        //LOG<<"RobotID=" << RobotID<<", RegionID="<<RegionID<<endl;
        SetTarget(loopFunctions->regionCenters[RegionID]);
        Stop(); //stop to reach the target location on the spiral path  
	  } 
      else // not holding food
	  {
	    ReachSpiralTargets(); /* Initializes targets positions. */
        //LOG<<"ReachSpiralTargets ***"<<endl;
	  }
  } 
  else if( DSA == RETURN_TO_REGION) 
  {
     // argos::LOG << "Robot "<< RobotID<< " RETURN_TO_REGION" << std::endl;
      SetIsHeadingToNest(true);
	  // Check if we reached the nest. If so record that we dropped food off and go back to the spiral
      if(IsInTheRegion()) 
      {
	    if (isHoldingFood)
	    {	
		//	argos::LOG << "Holding food and drop it" << std::endl;
			num_targets_collected++;
			loopFunctions->setScore(num_targets_collected);
		loopFunctions->currNumCollectedFood++;
			//num_targets_per_min++;

			//if (int(loopFunctions->getSimTimeInSeconds())%60 == 0){
		//		loopFunctions->foodPerMinute.push_back(num_targets_per_min);
		//		num_targets_per_min = 0;
		//	}	
	      
			DSA = RETURN_TO_SEARCH;
			SetIsHeadingToNest(false);
			SetTarget(ReturnPosition);
			isHoldingFood = false;
		}
	    else 
	    {}
	      
	  //ofstream results_output_stream;
	  //results_output_stream.open(results_full_path, ios::app);
	  //results_output_stream << loopFunctions->getSimTimeInSeconds() << ", " << ++num_targets_collected << ", " << "Col Count" << endl;	    
	  //results_output_stream.close();
	  
	}
    else
	{
	} //end of checking reached the nest
  } 
  else if(DSA == RETURN_TO_SEARCH) 
  {
   //LOG << "Robot "<< RobotID<< " RETURN_TO_SEARCH" << std::endl;
    SetIsHeadingToNest(false);
      
    // Check if we have reached the return position
    if ( IsAtTarget() )
	{
		SetIsHeadingToNest(false);
		SetTarget(ReturnSpiralPosition);
	  DSA = SEARCHING;
	}
  }
  else if(DSA == IDLE)
  {
	//LOG<<"Going to idle ..."<<endl;
	
      // Check if we reached the nest. If so record that we dropped food off and go back to the spiral
      if((GetPosition() - loopFunctions->regionCenters[PreRegionID]).SquareLength() < loopFunctions->RegionRadiusSquared) 
      { 
		Stop();
		}
  } 
  
  Move();
}   

/*****
 * Helper function that reads vector <char> pattern
 * and sets the target's direction base on the 
 * char at the current vector index.
 *****/
 void DSA_controller::ReachSpiralTargets(){

   /* If the robot hit target and the patter size >0
       then find the next direction. */
 
    if(TargetHit() && robotSpiralPoints.size() > 0) {
      /* Finds the last direction of the spiral. */
        nextSpiralPoint = robotSpiralPoints[robotSpiralPoints.size() - 1]; 
        SetIsHeadingToNest(false);
        
        if(firstAssigned)
        {
			//LOG<<"Robot " << RobotID<< ", firstAssigned="<< firstAssigned<< endl;
			//LOG<< "secondSpiralTarget[" << RegionID << "]=" << loopFunctions->secondSpiralTarget[RegionID]<<endl;
			//LOG<< "nextSpiralPoint= "<< nextSpiralPoint<<endl;
			if(loopFunctions->secondSpiralTarget[RegionID] == nextSpiralPoint)
			{
				PreRegionID = RegionID;
		        SetTarget(nextSpiralPoint);
    	        robotSpiralPoints.clear();
    	        //LOG<<"Robot " << RobotID<< " is the first, it will meet the second robot. So, go to the center" << endl; 
			}
			else
			{
				loopFunctions->firstSpiralTarget[RegionID] = nextSpiralPoint; //report the current spiral target location. Then, it can be used for sharing the path
				SetTarget(nextSpiralPoint);
				robotSpiralPoints.pop_back();
				//LOG<<"Robot " << RobotID<< ", update firstSpiralTarget to "<< nextSpiralPoint<<endl;
				//LOG<<"firstSpiralTarget[" << RegionID<< "] ="<<loopFunctions->firstSpiralTarget[RegionID]<<endl; 
			}
        }
	    else if(secondAssigned)
	    {
			//LOG<<"Robot " << RobotID<< ", secondAssigned=true"<<endl;
			//LOG<< "firstSpiralTarget[" << RegionID << "]=" << loopFunctions->firstSpiralTarget[RegionID]<<endl;
			//LOG<< "nextSpiralPoint= "<< nextSpiralPoint<<endl;
			if(loopFunctions->firstSpiralTarget[RegionID] == nextSpiralPoint)
			{
				PreRegionID = RegionID;
		        SetTarget(nextSpiralPoint - CVector2(0.3, 0.3));//can not set it to (0,0) since the TargetHit will avoid to go to the center.
    	        robotSpiralPoints.clear();
    	        
    	       // LOG<<"Robot " << RobotID<< " is the second, it will meet the first robot. So, go to the center" << endl;
			}
			else
			{
				loopFunctions->secondSpiralTarget[RegionID] = nextSpiralPoint; //report the current spiral target location.
				SetTarget(nextSpiralPoint);
				robotSpiralPoints.pop_back();
				//LOG<<"Robot " << RobotID<< ", update secondSpiralTarget to "<< nextSpiralPoint<<endl;
				//LOG<< "secondSpiralTarget[" << RegionID<< "]=" <<loopFunctions->secondSpiralTarget[RegionID]<<endl;
			}
		}
    }
    else if(TargetHit() && robotSpiralPoints.size() == 0) 
    {
        //LOG<<"Robot "<< RobotID<< " completed the spiral search ***"<<endl;
        PreRegionID = RegionID;
		DSA = START;
    	SetTarget(CVector2(0.1, 0.1)); //can not set it to (0,0) since the TargetHit will avoid to go to the center.
      }
}

/*****
 * Returns a boolean based on weather the robot is with 0.01 
 * distance tolerance. Declares that the robot had reached 
 * current target.
 *****/
 bool DSA_controller::TargetHit() {
    CVector2 position = GetPosition() - GetTarget();
    bool hit = false;
     
   // if(GetTarget() == loopFunctions->NestPosition && IsInTheNest())
    if(GetTarget() == loopFunctions->NestPosition)
    {
		hit = true;
		}
    else if(position.SquareLength() < TargetDistanceTolerance){
        hit = true;
    }
    return hit;
 }

/*****
 * Check if the Robot is finding food. This is defined as the Robot being within
 * the distance tolerance of the position of a food item. If the Robot has found
 * food then the appropriate boolean flags are triggered.
 *****/
void DSA_controller::SetHoldingFood(){
    if(IsHoldingFood() == false) 
      {
	if(rand()*1.0/RAND_MAX < ProbTargetDetection) {
	
        vector <CVector2> newFoodList; 
        size_t i = 0;
        for (i = 0; i < loopFunctions->FoodList.size(); i++)
	    {
         if ((GetPosition()-loopFunctions->FoodList[i]).SquareLength() < SquaredFoodDistanceTolerance && !isHoldingFood)
	      {
			isHoldingFood = true;
	      } 
	     else 
	      {
			newFoodList.push_back(loopFunctions->FoodList[i]);
	      }
        } 
        loopFunctions->FoodList = newFoodList;
      }
      }

}

/*****
 * Is this Robot_controller holding food?
 *     true  = yes
 *     false = no
 *****/
bool DSA_controller::IsHoldingFood() {
    return isHoldingFood;
}
/*****
 * After pressing the reset button in the GUI, this controller will be set to
 * default factory settings like at the start of a simulation.
 *****/
void DSA_controller::Reset() {
    collisionDelay  = 0;
    SetIsHeadingToNest(true);
    SetTarget(loopFunctions->NestPosition);
    CopyPatterntoTemp();
    
}

REGISTER_CONTROLLER(DSA_controller, "DSA_controller")