TODO:
- haar cascade with possibly multi character detection?
- filter out "background"

option a:
- filter out "background"
- track person until <condition> satisfied
- additionally can do expression detection and give that input
    - that can prompt rl or do a simple heuristic based ragebait
- find a new person to repeat

option b:
- track multiple ppl and label 


things to try:
person detection in a picture
- hog
- yolo

(then choose a random person)

track person
- yolo optimized - DONE
- reid paper (uses sam internally)
- fast sam
- filter out background - basically train the model to detect the background n ignore it?
- haar cascade - kinda done
- hog

detect face emotion
- finetune / transfer learn from fer plus dataset
- deepface + keras
