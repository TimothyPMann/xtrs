#!/bin/sh

arrayitem () {
  INDEX=$1
  shift
  eval echo \$$INDEX;
}

arraycount () {
  echo $#;
}

DONE=no
CONTROL_FILE=.cassette.ctl
DEFAULT_FORMAT=1
FORMAT_NAME='cas cpt wav direct debug'

if [ ! -e "$CONTROL_FILE" ]; then
  echo Creating $CONTROL_FILE
  echo "cassette.$(arrayitem $DEFAULT_FORMAT $FORMAT_NAME) 0 $DEFAULT_FORMAT" > \
   $CONTROL_FILE
fi

while [ "$DONE" != "yes" ]; do
  CONTROL=$(cat $CONTROL_FILE)
  FILENAME=$(arrayitem 1 $CONTROL)
  POSITION=$(arrayitem 2 $CONTROL)
  if [ $(arraycount $CONTROL) -lt 3 ]; then
    FORMAT=$DEFAULT_FORMAT
  else
    FORMAT=$(arrayitem 3 $CONTROL)
  fi
  if [ -e "$FILENAME" ]; then
    ISNEW=
  else
    ISNEW=" (new)"
  fi
  echo
  echo "Tape loaded: $FILENAME$ISNEW"
  echo 'Type:        '$(arrayitem $FORMAT $FORMAT_NAME)
  echo 'Position:    '$POSITION
  echo
  echo -n 'Command: '
  read COMMAND

  if [ $(arraycount $COMMAND) -lt 1 ]; then
    COMMAND=help
  fi

  TOKEN1="$(arrayitem 1 $COMMAND)"

  case $TOKEN1 in
    pos) ;;
    load|file)
      if [ $(arraycount $COMMAND) -ne 2 ]; then
        echo "Must specify a file name"
      else
        TOKEN2="$(arrayitem 2 $COMMAND)"
        case $TOKEN2 in
          *.cas|*.bin)
            FORMAT=1 ;;
          *.cpt)
            FORMAT=2 ;;
          *.wav)
            FORMAT=3 ;;
          /dev/dsp*)
            FORMAT=4 ;;
          *.debug)
            FORMAT=5 ;;
          *)
            FORMAT=1 ;;
        esac
        echo "$(arrayitem 2 $COMMAND) $FORMAT" > $CONTROL_FILE
      fi ;;
    type)
      if [ $(arraycount $COMMAND) -ne 2 ]; then
        echo Types are:
        echo '  '$FORMAT_NAME
      else
        TOKEN2="$(arrayitem 2 $COMMAND)"
        case $TOKEN2 in
          cas)
            FORMAT=1 ;;
          cpt)
            FORMAT=2 ;;
          wav)
            FORMAT=3 ;;
          direct)
            FORMAT=4
            FILENAME=/dev/dsp
            POSITION=0 ;;
          debug)
            FORMAT=5 ;;
          *)
            echo Types are:
            echo '  '$FORMAT_NAME ;;
        esac
        echo "$FILENAME $POSITION $FORMAT" > $CONTROL_FILE
      fi ;;
    rew)
      if [ $(arraycount $COMMAND) -lt 2 ]; then
        POSITION=0
      else
        POSITION=$(arrayitem 2 $COMMAND)
      fi
      echo "$FILENAME $POSITION $FORMAT" > $CONTROL_FILE ;;
    ff)
      if [ $(arraycount $COMMAND) -lt 2 ]; then
        POSITION=$(wc -c $FILENAME)
      else
        POSITION=$(arrayitem 2 $COMMAND)
      fi
      echo "$FILENAME $POSITION $FORMAT" > $CONTROL_FILE ;;
    quit|exit|done)
      DONE=yes ;;
    *)
      echo Commands are:
      echo '  'pos
      echo '  'load filename
      echo '  'type {$FORMAT_NAME}
      echo '  'rew [position]
      echo '  'ff [position]
      echo '  'quit ;;
    esac
done

exit
