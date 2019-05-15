//#include "Settings.h"
#include "C3_Pilot.h"
#include <due_can.h>
#include <Can_Protocol.h>

long speed_mmPs;
long turn_direction;
long pre_desired_speed;
long pre_turn_angle;
CAN_FRAME output; //CAN frame to carry message to C2

//Waypoint path[MAX_WAYPOINTS];  // course route to goal/mission
Waypoint path[3]; //3 is hardcoded
Waypoint estPos, path0, path1, path2;
String wrdState[] = { "STOP", "STRAIGHT", "ENTER_TURN", 
    "LEAVE_TURN", "APPROACH_GOAL", "LEAVE_GOAL"}; //used for clearer debugging

int next = 1; //index to path in a list
//last is the the last index of the Path/goal
int last_index_of_path = 2; //hardcode path of the last index/dest to 3 [cur,loc1,goal]
int q = 0; //for testing to vary speed in C3 find_state
bool first = true;
//******************** hard coded alternating speeds for testing ******************
int speeds[] = {2000, 2500, 1500, 2500, 1000, 2000};
int angg[] = {100, 125, 100, 120, 110, 122};
int speedIndex = 0;
int insaneCounter = 0;
//*********************************************************************************

/******************************************************************************************************
 * constructor
 *****************************************************************************************************/
C3_Pilot::C3_Pilot() {
  //Trike state starts Straight
  if(DEBUG) Serial.println("Entered C3 setup");
  state = STRAIGHT;
  //the path is set to approach the first intersection at index 1
  next = 1;
  speed_mmPs = DESIRED_SPEED_mmPs;
  turn_direction = 0;
  
  //CAN FRAME initialization
  output.length = MAX_CAN_FRAME_DATA_LEN_16;
  output.id = HiDrive_CANID; //Drive instructions from hilevel board
  
  populate_path(); //hard code path setup
}
/******************************************************************************************************
 * tuning_radius_mm(long) 
 *  This method computes the turning radius for the trike
 *  Not using the speed at the moment. Needs improvment for futher development
 *  by taking in the speed
 *  param :
 *  return :
 *****************************************************************************************************/
long C3_Pilot::turning_radius_mm(long speed_mmPs) {
  return  MIN_TURNING_RADIUS_MM; 
}

/******************************************************************************************************
 * test_past_destination(int)  
 *   This method checks to see if we have passed the destination and missed the turn.
 *   Also takes in consider of the type of path shape such as rectagle, skinny, square from current location
 *   to the intersection when determing if we passed the destination.
 *   param : n = next -> index of the intersection you're approaching
 *  return true = past the destination , false = Have not past the destination
 *****************************************************************************************************/
bool C3_Pilot::test_past_destination(int n) {
  if (abs(path[n - 1].east_mm - path[n].east_mm) > abs(path[n - 1].north_mm - path[n].north_mm)) {
    if (path[n].east_mm > path[n - 1].east_mm && estPos.east_mm > path[n].east_mm) {
      return true;
    } else if (path[n - 1].east_mm > path[n].east_mm && estPos.east_mm < path[n].east_mm) {
      return true;
    }
  } else {
    if (path[n].north_mm > path[n - 1].north_mm && estPos.north_mm > path[n].north_mm) {
      return true;
    } else if (path[n - 1].north_mm > path[n].north_mm && estPos.north_mm < path[n].north_mm) {
      return true;
    }
  }
  return false;
}

/******************************************************************************************************
 * test_approach_intersection(long, int)  
 *  This method test for intersection from straight to entering turn
 *  param : turning radius of the vehicle
 *  param : n = next -> index of the intersection you're approaching
 *  return : true -> if approached intersection to entering turn, false -> otherwise
 *****************************************************************************************************/
bool C3_Pilot::test_approach_intersection(long turn_radius_mm, int n) {
  Serial.println("Entering");
  Serial.print(abs(path[n - 1].east_mm - path[n].east_mm));
  Serial.print(" > ");
  Serial.print(abs(path[n - 1].north_mm - path[n].north_mm));
  Serial.print(" = " );
  Serial.println(abs(path[n - 1].east_mm - path[n].east_mm) > abs(path[n - 1].north_mm - path[n].north_mm));
  //if the distance from E to W is greater than  N to S
  if (abs(path[n - 1].east_mm - path[n].east_mm) > abs(path[n - 1].north_mm - path[n].north_mm)) {
    Serial.println("Further from E:W than N:S");
    Serial.print("New path further east than old? ");
    Serial.println(path[n].east_mm > path[n - 1].east_mm);
    Serial.print("Estimated path East: " + String(estPos.east_mm));
    Serial.println("   New path East: " + String(path[n].east_mm) + "  Turn Rad: " + String(turn_radius_mm));
    Serial.print("Estimate east larger than difference those 2?? ");
    Serial.println(estPos.east_mm >= path[n].east_mm - turn_radius_mm);
    Serial.println("-_-_-__---__-_--_--_-_-___-_---____---_____--____--_-___");
    //if destination is further east than current location AND current location is further East than destination - turn radius
    if (path[n].east_mm > path[n - 1].east_mm && estPos.east_mm >= path[n].east_mm - turn_radius_mm) {
      return true;
    }  //if dest is further west than current AND current location is further west than destination + turn radius 
    else if (path[n - 1].east_mm > path[n].east_mm &&
               estPos.east_mm <= path[n].east_mm + turn_radius_mm) {
        Serial.println("1B");        
        return true;
    }
  } else { //Bigger difference between current and destination N/S than E/W
     //if destination is North AND Destination - turn radius is less mm than current location
    if (path[n].north_mm > path[n - 1].north_mm &&
        estPos.north_mm >= path[n].north_mm - turn_radius_mm) {
      return true;
      // destination is south AND Destination + turn radius is greater than current location
    } else if (path[n - 1].north_mm > path[n].north_mm &&
               estPos.north_mm <= path[n].north_mm + turn_radius_mm) {
      Serial.println("2B");                
      return true;
    }
  }
  Serial.println("FAILED: ");
   Serial.println(path[n].east_mm);
      Serial.println(" > " );
      Serial.println(path[n - 1].east_mm);
      Serial.println(estPos.east_mm);
      Serial.println(" >= " );
      Serial.println(path[n].east_mm - turn_radius_mm);

      
  Serial.println(path[n].north_mm);
      Serial.println(" > " );
      Serial.println(path[n - 1].north_mm);
      Serial.println(estPos.north_mm);
      Serial.println(" >= " );
      Serial.println(path[n].north_mm - turn_radius_mm);
      
  Serial.println("path n: east " + String(path[n].east_mm) + " north " + String(path[n].north_mm));
  Serial.println("path n-1: east " + String(path[n-1].east_mm) + " north " + String(path[n-1].north_mm));
  return false;
}

/******************************************************************************************************
 * test_leave_intersection(long, int) 
 *  This method test for intersection from leaving turn to straight
 *  param : turning radius of the vehicle
 *  param : n = next -> index of the intersection you're approaching
 *  return : true -> if approached intersection to leave turn, false -> otherwise
 *****************************************************************************************************/
bool C3_Pilot::test_leave_intersection(long turning_radius_mm, int n) {
    //more change in east
    if (abs(path[n - 1].east_mm - path[n].east_mm) > abs(path[n - 1].north_mm - path[n].north_mm)) {
        if (path[n].east_mm > path[n - 1].east_mm) {
            if(abs((abs(path[n].north_mm) + turning_radius_mm) - (abs(estPos.north_mm) + turning_radius_mm)) >= turning_radius_mm/2){
            return true;
            }
        }
        else if(path[n - 1].east_mm > path[n].east_mm) {
            if(abs((abs(path[n].north_mm) + turning_radius_mm) - (abs(estPos.north_mm) + turning_radius_mm)) >= turning_radius_mm/2) {
            return true;
            }
        }
    }
    else {
        //more change in north
        if(path[n].north_mm > path[n - 1].north_mm){
            if(abs((abs(path[n].east_mm) + turning_radius_mm) - (abs(estPos.east_mm) + turning_radius_mm)) >= turning_radius_mm/2){
                return true;
            }
        }
        else if(path[n - 1].north_mm > path[n].north_mm){
            if(abs((abs(path[n].east_mm) + turning_radius_mm) - (abs(estPos.east_mm) + turning_radius_mm)) >= turning_radius_mm/2) {
                return true;
            }
        }
    }
    return false;
}

/******************************************************************************************************
 * get_turn_direction_angle(int)  
 *  This method determines the direction of turns of the vehicle of either left 
 *  or right once approached a turn intersection. Angle returned is either 90 for right 
 *  or -90 for left.  
 *  param : n = next -> index of the intersection you're approaching
 *  return : positive = right, negative = left, 0 = straight
 *****************************************************************************************************/
int C3_Pilot::get_turn_direction_angle(int n) {

  if (state == ENTER_TURN) {
    int turn_direction_angle = 0;
    double turn_dir = (estPos.Evector_x1000 * path[n].Evector_x1000) + (estPos.Nvector_x1000 * path[n].Nvector_x1000);
    double x_magnitude = sqrt((estPos.Evector_x1000 * estPos.Evector_x1000) + (estPos.Nvector_x1000 * estPos.Nvector_x1000));
    double y_magnitude = sqrt ((path[n].Evector_x1000 * path[n].Evector_x1000) + (path[n].Nvector_x1000 * path[n].Nvector_x1000));
    double dot_product = (turn_dir / (x_magnitude * y_magnitude));
    dot_product /= 1000000.0;
    turn_dir = acos(dot_product); // angle in radians
    double cross = (estPos.Evector_x1000 * path[n].Nvector_x1000) - (estPos.Nvector_x1000 * path[n].Evector_x1000);
    turn_direction_angle = turn_dir * 180 / PIf; //angle is degrees
    if (cross > 0)
      turn_direction_angle = -turn_direction_angle;
      
    return turn_direction_angle;
  }
  return 0;
}

/******************************************************************************************************
 * find_state(long, int)  
 *  The method find and set the state of the trike and
 *  also set the speed of the trike depending on the state it's in
 *  param : turning radius_mm
 *  param : n = next -> index of the intersection you're approaching
 *****************************************************************************************************/
void C3_Pilot::find_state(long turn_radius_mm, int n) {
  Serial.println("entering state");
  switch (state) {
    case STRAIGHT:
      q++;
      if(q % 3 == 0)
        speed_mmPs = DESIRED_SPEED2;
      else 
        speed_mmPs = DESIRED_SPEED_mmPs;
      if (test_approach_intersection(turn_radius_mm, n)) {
        //last index of path/goal
        if (n == last_index_of_path) {
          state = APPROACH_GOAL;
        }
        else {
          state = ENTER_TURN;
        }
      }
      break;

    case STOP:
      speed_mmPs = 0;
      break;

    case ENTER_TURN:
      //setting to turning speed
      speed_mmPs = turn_speed;

      if (test_leave_intersection(turn_radius_mm, n)) {
        state = STRAIGHT;
        next++;

      }
      break;

    case APPROACH_GOAL:
      if (test_past_destination(n)) {
        if(DEBUG3) Serial.println("Reached Goal!");
        state = STOP;
      }
      break;
  }

}

/******************************************************************************************************
 * hardCoded_Pilot_Test()
 *   Tests the code by cycling through a hard coded speed and angle array to send to lowlevel
 *   this can be used to observe the bike repsonding to different speeds and angles to 
 *   ensure proper CAN communication and response.
 *   Currently contains two tests, A & B. 
 *   Comment out one based on if using delays in your code for debugging
 *****************************************************************************************************/
void C3_Pilot::hardCoded_Pilot_Test() {
  speed_mmPs = speeds[speedIndex];
  turn_direction = angg[speedIndex];

//test section A. to be used if no delays are in place so speed and angle not changed constantly
 /* if(insaneCounter < 6000) {
      insaneCounter++;
  }
  else {
    if(speedIndex == 5)
      speedIndex = 0;
    else
      speedIndex++;
    insaneCounter = 0;  
    Serial.println("Speed: " + String(speeds[speedIndex]));
  }
 // Serial.println("insaneCounter = " + String(insaneCounter) + ", Index = " + String(speedIndex));
 */

 //test section B. to be used if delays are in place so angle will speed/angle will change occasionally
  if(speedIndex == 5)
    speedIndex = 0;
  else
    speedIndex++; 
}

/******************************************************************************************************
 * C3_communicate_C2()
 * Transmits the data over CAN BUS from C3_Pilot to C2_Lowlevel
 * Sends a speed in mm/s and a turn angle in degrees
 *****************************************************************************************************/
void C3_Pilot::C3_communicate_C2() {
  //if (pre_desired_speed != speed_mmPs || pre_turn_angle != turn_direction) {  send every time, lowlevel only reacts when not close
    
    output.data.low = speed_mmPs;
    output.data.high = turn_direction;
        
    if(DEBUG2)
      Serial.println("**Sending: Speed: " + String(output.data.low) + " Angl: " + (output.data.high));
    
    CAN.sendFrame(output); //send the message
    delay(1000);
   /* sends++;
    if(sends > 1000) {
      Serial.println("1k sends at " + String(millis()));
      sends = 0;
    } */

    //keep track of previous data to compare next loop
    pre_desired_speed = speed_mmPs;
    pre_turn_angle = turn_direction;
}




/*****************************************************************************************************
 * update()
 * Determines state of trike, uses this to get a disered speed and angle
 * and transmits this data to the C2_Lowlevel board
 * param: estimated position & oldPosition Waypoints
 *****************************************************************************************************/
void C3_Pilot::update(Waypoint &estimated_position, Waypoint &oldPos) {
  estPos = estimated_position; //copy estimated position
  if(first) initializePosition(oldPos);
  //Determining the state of the Trike
  find_state(TURN_RADIUS_MM, next);
  if(DEBUG3) Serial.println("State is: " + String(wrdState[state]) + ", path # " + String(next));
  //comment out and uncomment lines below when not in testing
  //hardCoded_Pilot_Test();
  
  //Determining the turn direction for the trike "left, right or straight" comment out if using test line above
  turn_direction = get_turn_direction_angle(next);
  if(DEBUG3) Serial.println("setting turn angle to: " + String(turn_direction));

  C3_communicate_C2(); //send data to lowlevel through CAN
  estimated_position = estPos; //copy back to main estimated_position
}

/*****************************************************************************************************
 * Destructor
 *****************************************************************************************************/
 C3_Pilot::~C3_Pilot() {
 }
 
 /********************************************************************************************************
 * populate_path
 *   Hard coded path for testing
 *******************************************************************************************************/
 void C3_Pilot::populate_path() {

    path0.east_mm =  6925;
    path0.north_mm = 5893;

    path1.east_mm = 11735;
    path1.north_mm = 5913;

    path2.east_mm = 17416;
    path2.north_mm = 18791;

    path[0] = path0;
    path[1] = path1;
    path[2] = path2;

    for (int i = 0; i < 3; i++) {
        path[i].vectors(&path[i+1]);
    }
}

/********************************************************************************************************
 * intializePosition()
 *   Hard code assigning estimated position to the first element in path
 *******************************************************************************************************/
  void C3_Pilot::initializePosition(Waypoint &oldPos) {
  estPos.east_mm = path[0].east_mm;
  estPos.north_mm = path[0].north_mm;
  estPos.Evector_x1000 = path[0].Evector_x1000;
  estPos.Nvector_x1000 = path[0].Nvector_x1000;
  oldPos = estPos;
  Serial.println("INITIAL!!!!!!!!!!!!!!!!!!!!");
  Serial.println("North: " + String(estPos.north_mm) + ", East: " + String(estPos.east_mm));
  first = false; //one time through this routine
} 
