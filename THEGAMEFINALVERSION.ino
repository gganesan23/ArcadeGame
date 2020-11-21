/* This code creates a game on the 8x8 LED display in which an upside-down-T shaped player is moved around using a joystick and can shoot by pressing a momentary switch.  The screen 
 *  constantly advances downwards (one row every 2 seconds).  There are T-shaped enemies in random locations that appear from the top of the screen every three rows, and these enemies 
 *  can shoot fast bullets down the screen.  If the player is hit by a bullet, they die.  Additionally, if the player is at the bottom of the screen, they will usually just stay in 
 *  their spot whenever the rest of the screen advances (so that they don't fall off the board), but if they cannot stay in their spot because something else on the board will be moving 
 *  there (e.g. an enemy), they also die.  Foes can be 'exploded' (made to disappear) if hit by a player shot.  The player has three lives, and the number of lives is displayed after 
 *  every death.  Once they have zero lives, a game over screen (of a dead smiley) will be displayed, and the game will restart with the player having three lives.
 *  
 *  The gameboard is represented as a 16x8 array though only the bottom 8 rows are displayed on the LED array.  Though the player cannot exist beyond the bottom 8 rows, the enemies can,
 *  and their shots can still travel down and hit the player even when they aren't visible on the LED array yet.  The player also can't shoot above the bottom 8 rows, and so before the
 *  enemy arrives on the visible screen, they can only shoot the enemy's shots to explode them or dodge them to survive.
 * 
 * Throughout, spots on the pattern array will be coded as such: 1 = player, 2 = enemy, 3 = enemy shot, 4 = player shot.
 * 
 * HARDWARE DETAILS:
 * 
 * The LED display has a connected grid of 64 LEDs soldered onto a PCB. Each row is connected to a separate Arduino anode pin, and each column is connected to a separate Arduino cathode pin 
 * (through the PCB).  There are PMOS transistors between the rows and their anode pins (as one row is turned on at once in display(), requiring a greater potential current through the row 
 * if all the LEDs in the row were to be on) and resistors at the bottom of the columns (to limit the current through each column, which then flows into the Arduino cathode pin).  There
 * can only be a maximum of one LED in a columnn on at a time, as the rows are turned on individually, and so no nMOS transistors are required.  The joystick used has potentiometers for 
 * the x and y directions, and a built-in momentary switch that is turned on by pressing it in the z direction.
 */

byte numLives;
byte pattern[16][8];
long lastAdvanced;
long lastFoeShot;
long lastFoeShotsMoved;
long lastPlayerShot;
long lastPlayerShotsMoved;
long lastPlayerMoved;
bool gameInProgress;
bool switchWasPressed;
bool switchPressed;
bool playerIsShooting;
bool firstShotSet; // determines which of two alternating sets of enemies is shooting at this time
int joystickX; // these will determine the number of spaces moved
int joystickY; 
byte playerX; // keeps track of player location on board for easy moving.  playerX and playerY represent the TOP of the player's upside-down-T shape
byte playerY;
byte advanceRoundNum; // determines whether to put foes on this row or not
bool playerLosesLife;

const byte SWITCH = 1;
const byte JOYSTICK_X = A4;
const byte JOYSTICK_Y = A5;
const byte ANODE_PINS[8] = {13, 12, 11, 10, 9, 8, 7, 6};
const byte CATHODE_PINS[8] = {A3, A2, A1, A0, 5, 4, 3, 2};
const byte ROWS_BETWEEN_FOES = 1;
const int SCREEN_ADVANCE_DELAY = 2000;
const int PLAYER_MOVE_DELAY = 500;
const int FOE_SHOOT_DELAY = 1000;
const byte PLAYER_SHOOT_DELAY = 50;
const int FOE_SHOT_MOVE_DELAY = 500;
const int PLAYER_SHOT_MOVE_DELAY = 500;
const int DEAD_SCREEN_SHOW_TIME = 2000;
const int SCORE_BLINK_SHOW_TIME = 500;
const byte MAX_LIVES = 3;
byte DEAD_SCREEN[16][8];
byte LIVES_3[16][8];
byte LIVES_2[16][8];
byte LIVES_1[16][8];
byte LIVES_0[16][8];

// This function does almost everything setup() does except for resetting numLives(), and so can be called every time the player dies even it's not the end of the entire game.
void initialize() {
  lastAdvanced = lastPlayerShot = lastFoeShot = lastPlayerShotsMoved = lastFoeShotsMoved = lastPlayerMoved = millis();
  switchWasPressed = switchPressed = playerIsShooting = playerLosesLife = false;
  firstShotSet = true;
  joystickX = joystickY = 0; // these will determine the number of spaces moved
  playerX = 3; // setting the player at the bottom ~center of the board
  playerY = 14;
  advanceRoundNum = 1; // determines whether to put foes on this row or not

  // initialize Arduino pins
  for (byte i = 0; i < 8; i++) {
    pinMode(ANODE_PINS[i], OUTPUT);
    digitalWrite(ANODE_PINS[i], HIGH);

    pinMode(CATHODE_PINS[i], OUTPUT);
    digitalWrite(CATHODE_PINS[i], HIGH);
  }

  byte DEAD_SCREENcopy[8][8] = // contains pattern to display dead smiley face for game over
  { {1, 0, 0, 1, 1, 0, 0, 1},
    {0, 1, 1, 0, 0, 1, 1, 0},
    {0, 1, 1, 0, 0, 1, 1, 0},
    {1, 0, 0, 1, 1, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 1, 1, 1, 0, 0},
    {0, 1, 0, 0, 0, 0, 1, 0},
    {1, 0, 0, 0, 0, 0, 0, 1}
  };

  byte LIVES_3copy[8] = {0, 1, 0, 1, 0, 1, 0, 0};
  byte LIVES_2copy[8] = {0, 1, 0, 1, 0, 0, 0, 0};
  byte LIVES_1copy[8] = {0, 1, 0, 0, 0, 0, 0, 0};
  byte LIVES_0copy[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  // initialize pattern, LIVES arrays (to display number of dots showing number of lives left after each time the player dies), and DEAD_SCREEN (dead smiley)
  for (byte i = 0; i < 16; i++) {
    for (byte j = 0; j < 8; j++) {
      pattern[i][j] = 0;
      if (i != 12) {
        LIVES_3[i][j] = LIVES_2[i][j] = LIVES_1[i][j] = LIVES_0[i][j] = 0;
      }
      else {
        LIVES_3[i][j] = LIVES_3copy[j];
        LIVES_2[i][j] = LIVES_2copy[j];
        LIVES_1[i][j] = LIVES_1copy[j];
        LIVES_0[i][j] = LIVES_0copy[j];
      }

      if (i < 8) {
        DEAD_SCREEN[i][j] = 0;
      } else {
        DEAD_SCREEN[i][j] = DEAD_SCREENcopy[i - 8][j];
      }
    }
  }

  //set player on board at starting position
  pattern[15][2] = pattern[15][3] = pattern[15][4] = pattern[14][3] = 1;

}

// This is run at when the code is started up as well as every time the game is to be restarted (after the player reaches 0 lives).
void setup() {
  Serial.begin(115200);
  initialize();
  numLives = MAX_LIVES;
  gameInProgress = true;
}

/* This function is looped by the Arduino.  At the end of each loop, if the player is still alive the gameboard is displayed on the LED array, and if the player is dead a screen that 
 *  displays how many lives they currently have is displayed instead.  If the player has lost all three of their provided lives, the game over screen is displayed, and the game is 
 restarted. */
void loop() {
  while (gameInProgress) {
      // update joystick value
      int joystickPosX = analogRead(JOYSTICK_X);
      int joystickPosY = analogRead(JOYSTICK_Y);

      joystickX = (joystickPosX < 400) ? -1 : (joystickPosX > 700 ? 1 : 0);
      joystickY = (joystickPosY < 400) ? -1 : (joystickPosY > 700 ? 1 : 0);

      // update switch value (for shooting)
      switchPressed = digitalRead(SWITCH);
      if (!switchWasPressed && switchPressed) { // player can't shoot if they're just holding down the shoot button
      playerIsShooting = true;
      }
      
    if (millis() > lastAdvanced + SCREEN_ADVANCE_DELAY) {
      advanceScreen();
      lastAdvanced = millis();
    }

    if (millis() > lastPlayerMoved + PLAYER_MOVE_DELAY) {
      Serial.print("Moving player x = ");
      Serial.print(joystickX);
      Serial.print(", y=");
      Serial.println(joystickY);
      movePlayer(joystickX, joystickY);
      lastPlayerMoved = millis();
    }

    if ((millis() > lastPlayerShot + PLAYER_SHOOT_DELAY) && playerIsShooting) {
      playerShoots();
      playerIsShooting = false;
      lastPlayerShot = millis();
    }
    
    if (millis() > lastFoeShot + FOE_SHOOT_DELAY) {
      allFoesShoot();
      lastFoeShot = millis();
    }
    
    if (millis() > lastPlayerShotsMoved + PLAYER_SHOT_MOVE_DELAY) {
      movePlayerShots();
      lastPlayerShotsMoved = millis();
    }
    
    if (millis() > lastFoeShotsMoved + FOE_SHOT_MOVE_DELAY) {
      moveFoeShots();
      lastFoeShotsMoved = millis();
    }
    
    if (playerLosesLife) { 
      Serial.print("NUMLIVES WAS ");
      Serial.println(numLives);
      numLives--;
      Serial.print("NUMLIVES IS ");
      Serial.println(numLives);
      
      // display blinking dots on the screen to indicate how many lives there WERE and how many lives there now ARE
      if (numLives == 2) {
        blinkingDisplay(LIVES_3, LIVES_2);
        initialize();
      }
      else if (numLives == 1) {
        blinkingDisplay(LIVES_2, LIVES_1);
        initialize();
      }
      else if (numLives == 0) {
        blinkingDisplay(LIVES_1, LIVES_0);
        gameInProgress = false;
      }
    }
    else {
      display(pattern); // if player hasn't just died, display board as normal
    }
    switchWasPressed = switchPressed;
    joystickX = 0;
    joystickY = 0;
  }
  // function enters here if gameInProgress becomes false: i.e. if the player has 0 lives and the game is to be ended
  long timeNow = millis();
  while (millis() < timeNow + DEAD_SCREEN_SHOW_TIME) { // display dead smiley face (game over screen)
    display(DEAD_SCREEN);
  }
  setup(); // reset game!
}

// This function causes the entire screen to advance one row down.
void advanceScreen() {
  if (playerY == 14) { // if the player is at the bottom of the board
    if (!movePlayer(0, -1)) { // try moving player up one spot (to later be moved down, as if hasn't moved at all).  If can't, player dies
      playerLosesLife = true;
      return;
    }
  }
  bool playerAlreadyMoved = false;
  for (int i = 15; i > 0; i--) { // from the last to second row, copy all entries down one row
    for (int j = 0; j < 8; j++) {
       if (pattern[i-1][j] == 1 && !playerAlreadyMoved) {
          playerY++; // need to change player position just once if player is moved down
          playerAlreadyMoved = true;
       }
      pattern[i][j] = pattern[i - 1][j];
    }
  }
  
  // now to make the first row...
  /* The rest of the code here determines which of three kinds of rows the new row is (containing the lower bits of foes, containing the upper parts of foes, or an empty row)
  and creates the corresponding values in the new row.*/
  if (advanceRoundNum == ROWS_BETWEEN_FOES + 1) { // in this case, new first row will have the tops of foes
    for (int i = 0; i < 8; i++) { // create the foe-tops in this row for any foe-bottoms in the row below
      if (pattern[0][i] == 2) { // sbould only be true for values of i from 1 to 6 inc.
        pattern[0][i - 1] = 2;
        pattern[0][i + 1] = 2;
        i++;
      }
      else {
        pattern[0][i] = 0; // nothing else in this new row
      }
    }
    advanceRoundNum = 0; // reset count
  }
  else {
    for (int i = 0; i < 8; i++) { // make row empty (don't care anymore abt values in the row below)
      pattern[0][i] = 0;
    }
    if (advanceRoundNum == ROWS_BETWEEN_FOES) { // in this case, new first row will have the bottoms of foes
      byte randPos = random(1, 7);
      pattern[0][randPos] = 2; // sets one random pos with x-val between 1 and 6 inc. to a foe bottom
      Serial.println("made a foe!");
    }
    advanceRoundNum++; // increase count
  }
}

// This function checks whether a certain change in the player's position can be made.  If it can, the function returns true and moves the player.  If not, it returns false.
bool movePlayer(int changeX, int changeY) {
  // check that this is a valid move for each spot the player occupies
  bool move1 = checkValidMove(playerX + changeX, playerY + changeY);
  bool move2 = checkValidMove((playerX - 1) + changeX, (playerY + 1) + changeY);
  bool move3 = checkValidMove(playerX + changeX, (playerY + 1) + changeY);
  bool move4 = checkValidMove((playerX + 1) + changeX, (playerY + 1) + changeY);
  if (!move1 || !move2 || !move3 || !move4) {
    return false; // can't move player there
  }

  // then actually shift player by:

  // first taking them out of the location they used to be in...
  pattern[playerY][playerX] = 0;
  pattern[playerY + 1][playerX - 1] = 0;
  pattern[playerY + 1][playerX] = 0;
  pattern[playerY + 1][playerX + 1] = 0;

  // then entering them into the new location...
  pattern[playerY + changeY][playerX + changeX] = 1;
  pattern[(playerY + 1) + changeY][(playerX - 1) + changeX] = 1;
  pattern[(playerY + 1) + changeY][playerX + changeX] = 1;
  pattern[(playerY + 1) + changeY][(playerX + 1) + changeX] = 1;

  // finally, shifting player coordinates!
  playerX = playerX + changeX;
  playerY = playerY + changeY;
  
  Serial.print("Moved player by x=");
    Serial.println(changeX);
    Serial.print(", y=");
    Serial.println(changeY);
  
  return true;
}

/* This function takes as input the intended final positions of a moving of the player.  If the move can be made, the player is moved and the function returns true; otherwise it returns
false. */
bool checkValidMove(int posX, int posY) { // input parameters are intended positions
  if (posX < 0 || posX > 7 || posY < 8 || posY > 15) { // player can't move off visible screen
    return false;
  }
  if (pattern[posY][posX] == 2) { // foe already in that spot
    return false;
  }
  if (pattern[posY][posX] == 3) { // foe shot in that spot, so player explodes
    playerLosesLife = true;
    return true;
  }
  return true; // if a player shot is in that spot OR the position is empty, the player can be moved there
}

// This function moves all enemy shots down the board one row (and is called in the loop() function periodically for the natural speed of the enemy shots).
void moveFoeShots() {
  for (int i = 15; i >= 0; i--) {
    for (byte j = 0; j < 8; j++) {
      if (pattern[i][j] == 3) { // if foe shot - to be moved DOWN
        Serial.print("There is a foe shot to move at x = ");
        Serial.print(j);
        Serial.print(", y = ");
        Serial.println(i);
        
        pattern[i][j] = 0; // remove from this spot in any case

        if (pattern[i + 1][j] == 1) { // if player there, player dies
          playerLosesLife = true;
          Serial.println("distant foeshot kills player!");
        }
        else if (pattern[i + 1][j] == 2) { // if foe there, foe shot just disappears
          Serial.println("Foeshot hit foe!");
        }
        else if (pattern[i + 1][j] == 4) { // if player shot there, both 'collide' and disappear
          pattern[i + 1][j] = 0;
        }
        else if (i != 15) { // as long as previous conditions don't apply AND enemy not trying to shoot past bottom of board, move shot down
          pattern[i + 1][j] = 3;
        }
      }
    }
  }
}

/* This function moves all player shots up the board one row (and is called in the loop() function periodically for the natural speed of the enemy shots). Unlike enemy shots, player
 * shots cannot travel above the visible board (the bottom 8 rows of pattern[][]).  
 */
void movePlayerShots() {
  for (byte i = 0; i < 16; i++) {
    for (byte j = 0; j < 8; j++) {
      if (pattern[i][j] == 4) { // if player's shot - to be moved UP

        pattern[i][j] = 0;  // remove from this spot in any case

        if (pattern[i - 1][j] == 1) { // if player there, player shot just disappears
          Serial.println("Playershot hit player!");
        }
        else if (pattern[i - 1][j] == 2) { // if foe there, foe is 'exploded' (made to disappear)
          goBoomFoe(i - 1, j);
        }
        else if (pattern[i - 1][j] == 3) { // if foe's shot here, both shots 'collide' and disappear
          pattern[i - 1][j] = 0;
        }
        else if (i != 8) { // can't move player shot past top of screen
          pattern[i - 1][j] = 4;
        }

      }
    }
  }
}

// This function causes enemies (including those above the visible board) to create a new shot in the space below them.  Half of all enemies shoot at once (alternating enemy rows).
void allFoesShoot() {

  // This first chunk of code just determines the index of the first row that contains shooting enemies.
  int advanceRoundNumInt = advanceRoundNum;
  int firstFoeRow = (1 + advanceRoundNumInt) % 3;
  if (!firstShotSet) {
    firstFoeRow += 3; // determines whether the first or second of the alternating sets of enemies will be shooting at this time
  }

  for (byte i = firstFoeRow; i < 15; i += 6) {
    for (byte j = 0; j < 8; j++) {
      if (pattern[i][j] == 2) { // locating foe's specific position on this row
        foeShoots(i, j);
      }
    }
  }
  firstShotSet = !firstShotSet; // ready to alternate shooting rows for next time
}

// This function causes ONE foe at a SPECIFIC position to create a shot in the space below it.
void foeShoots(byte row, byte col) {
  if (row == 15) { // foe can't shoot below screen
    Serial.println("Foe trying to shoot below screen!");
  }
  else if (pattern[row + 1][col] == 1) { // if player there, player dies
    playerLosesLife = true;
    Serial.println("PLAYER HIT BY CLOSE FOESHOT!");
  }
  else {
    pattern[row + 1][col] = 3; // otherwise, create shot below foe
  }
}

// This function allows the player to shoot.  The player cannot shoot above the visible screen (bottom 8 rows).
void playerShoots() {
  Serial.println("Player is shooting!");
  if (playerY == 8) { // player can't shoot above visible screen
     Serial.println("Player trying to shoot above visible screen!");
  }
  else if (pattern[playerY - 1][playerX] == 2) { // if foe there, foe is 'exploded' (made to disappear)
    goBoomFoe(playerY - 1, playerX);
  }
  else if (pattern[playerY - 1][playerX] == 3) { // if foe's shot here, both 'collide' and disappear
    pattern[playerY - 1][playerX] = 0;
  }
  else {
    pattern[playerY - 1][playerX] = 4; // otherwise, shot is created in space above player
  }
}

/* Given one position that contains part of a foe that is to be 'exploded' (made to disappear from pattern[][]), this function finds the other positions this foe occupies and sets them
all to 0.  The provided position is definitely at a row 8 or higher, since the only thing that makes foes explode is player shots and these player shots can't make it above row 8.
*/
void goBoomFoe(byte row, byte col) { // row definitely 8 or higher
  for (byte i = row - 1; i <= row + 1; i++) {
    for (byte j = 0; j < 8; j++) {
      if (pattern[i][j] == 2) {
        pattern[i][j] = 0; // in the rows below and under where this foe was hit, remove any sign of foe (this works out as foes exist at every third row)
      }
    }
  }
}


// This function displays the board pattern on the LED array.
void display(byte toShow[16][8]) {
  for (byte i = 8; i < 16; i++) {
    for (byte j = 0; j < 8; j++) {
      if (toShow[i][j] != 0) { // if any value other than 0 in spot, light should be on
        digitalWrite(CATHODE_PINS[j], LOW);
      } else { // light should be off
        digitalWrite(CATHODE_PINS[j], HIGH);
      }
    }
    digitalWrite(ANODE_PINS[i - 8], LOW); // turn on row
    delay(1);
    digitalWrite(ANODE_PINS[i - 8], HIGH); // turn off row
  }
}

/* This function is used to blink between the arrays for the score (e.g. if the number of lives goes from 3 to 2, the arrays with 3 and 2 dots will be displayed alternatingly to
visually demonstrate the life being lost. */
void blinkingDisplay(byte arr1[16][8], byte arr2[16][8]) {
  long timeNow = millis();
  while (millis() < timeNow + SCORE_BLINK_SHOW_TIME) { // display blank screen for short period first
    display(LIVES_0);
  }

  for (byte i = 0; i < 3; i++) {
    timeNow = millis();
    while (millis() < timeNow + SCORE_BLINK_SHOW_TIME) { // first array displayed for short period of time
      display(arr1);
    }
    timeNow = millis();
    while (millis() < timeNow + SCORE_BLINK_SHOW_TIME) { // second array displayed for short period of time
      display(arr2);
    }
  }

  timeNow = millis();
  while (millis() < timeNow + SCORE_BLINK_SHOW_TIME) { // display blank screen for short period before game begins again
    display(LIVES_0);
  }
}
