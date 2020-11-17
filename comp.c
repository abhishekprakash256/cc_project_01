#include "y.tab.h"
#include "containers.h"
int yyparse();

// symbol table
struct node_fun_str* fun_r = NULL;
struct node_fun_str* fun_t = NULL;
struct node_var_str* vars_r = NULL;
struct node_var_str* vars_t = NULL;

// br structure
struct node_istr* ifun_r = NULL;
struct node_istr* ifun_t = NULL;

// semantic checks

bool is_term (struct ast* node){
  int t = node->ntoken;
  if (t == CONST || t == PLUS || t == MINUS || t == MULT || t == DIV || t == MOD) return true;
  if (t == VARID) {
    struct node_var_str* tmp = find_var_str(node->id, node->token, vars_r);
    if (tmp != NULL && INTID == tmp->type) return true;
  }
  if (t == CALL && NULL != find_fun_str(node->token, fun_r) && INT == find_fun_str(node->token, fun_r)->type) return true;
  if (t == IF) return is_term (get_child(node, 2));
  if (t == LET) return is_term (get_child(node, 3));
  return false;
}

bool is_fla (struct ast* node){
  int t = node->ntoken;
  if (t == TRUE || t == FALSE || t == EQ || t == LE || t == LT || t == GT || t == GE || t == NOT || t == AND || t == OR) return true;
  if (t == VARID) {
    struct node_var_str* tmp = find_var_str(node->id, node->token, vars_r);
    if (tmp != NULL && BOOLID == tmp->type) return true;
  }
  if (t == CALL && NULL != find_fun_str(node->token, fun_r) && BOOL == find_fun_str(node->token, fun_r)->type) return true;
  if (t == IF) return is_fla (get_child(node, 2));
  if (t == LET) return is_fla (get_child(node, 3));
  return false;
}

int get_var_type(struct ast* node, struct ast* end){
  struct ast* tmp = node;
  while (tmp != end){
    if (strcmp(tmp->token, node->token) == 0){
      int t = tmp->parent->ntoken;
      if (t == EQ || t == LE || t == LT || t == GT || t == GE || t == PLUS || t == MINUS || t == MULT || t == DIV || t == MOD){
        return INTID;
      }
      else if (t == NOT || t == AND || t == OR) {
        return BOOLID;
      }
    }
    tmp = tmp->next;
  }
  return -1;
}

int get_fun_var_types(struct ast* node){
  if (node->ntoken == FUNID){
    char* fun_id = node->token;
    if (find_fun_str(fun_id, fun_r) != NULL){
      printf("Function %s defined twice\n", fun_id);
      return 1;
    }

    if (strcmp(node->token, "EVAL") == 0 || strcmp(node->token, "PRINT") == 0) return 0;

    int arg_num = get_child_num(node->parent);
    struct ast* body = get_child(node->parent, arg_num);

    struct node_var_str* args = NULL;
    for (int i = 2; i < arg_num; i++) {
      struct ast* var = get_child(node->parent, i);
      if (find_fun_str(var->token, fun_r) != NULL || strcmp(var->token, fun_id) == 0){
        printf("Variable and function %s have the same name\n", var->token);
        return -1;
      }
      if (find_var_str(var->id, var->token, vars_r) != NULL){
        printf("Variable %s declared twice in function %s\n", var->token, fun_id);
        return -1;
      }
      int type = get_var_type(var, body);
      if (type == -1){
        printf("Unable to detect type of argument %s\n", var->token);
        return -1;
      }
      push_var_str(var->id, body->id, type, var->token, &vars_r, &vars_t);
      if (args == NULL) args = vars_t;
    }
    if (is_term(body)) {
      push_fun_str(fun_id, INT, arg_num - 2, args, &fun_r, &fun_t);
    }
    else if (is_fla(body)) {
      push_fun_str(fun_id, BOOL, arg_num - 2, args, &fun_r, &fun_t);
    }
    else {
      printf("Unable to detect type of function %s\n", fun_id);
      return -1;
    }
  }
  if (node->ntoken == LETID) {
    if (find_var_str(node->id, node->token, vars_r) != NULL){
      printf("Variable %s declared twice\n", node->token);
      return -1;
    }
    int type;
    if (is_fla(get_child(node->parent, 2))) type = BOOLID;
    else if (is_term(get_child(node->parent, 2))) type = INTID;
    else {
      printf("Unable to detect type of variable %s\n", node->token);
      return -1;
    }
    push_var_str(node->id, get_child(node->parent, 3)->id, type, node->token, &vars_r, &vars_t);
  }
  return 0;
}

int type_check(struct ast* node){
  int t = node->ntoken;
  int needs_term = (t == EQ || t == LE || t == LT || t == GT || t == GE || t == PLUS || t == MINUS || t == MULT || t == DIV || t == MOD);
  int needs_fla  = (t == NOT || t == AND || t == OR);
  struct ast_child* temp_child_root = node -> child;
  while (temp_child_root != NULL) {
    struct ast* child_node = temp_child_root->id;

    if ((needs_term && !is_term(child_node)) ||
        (needs_fla && !is_fla(child_node))) {
      printf("Does not type check: operator %s vs operand %s\n", node->token, child_node->token);
      return 1;
    }
    temp_child_root = temp_child_root -> next;
  }

  if (t == IF) {
    if (!is_fla (get_child(node, 1))) {
      printf("Does not type check: the if-guard %s is not a formula\n", get_child(node, 1)->token);
      return 1;
    }
    else if ((is_fla (get_child(node, 2)) && !is_fla (get_child(node, 3))) ||
             (is_term (get_child(node, 2)) && !is_term (get_child(node, 3)))) {
      printf("Does not type check: the then-branch %s and the else-branch %s should have the same type\n", get_child(node, 2)->token, get_child(node, 3)->token);
      return 1;
    }
  }

  if (node->ntoken == DEFFUN) {
    char* fun_id = get_child(node, 1)->token;
    struct node_fun_str* deffun = find_fun_str(fun_id, fun_r);
    if (NULL == deffun){
      printf("Function %s has not been defined\n", fun_id);
      return 1;
    }
  }

  if (node->ntoken == CALL) {
    char* call_id = node->token;
    struct node_fun_str* fun = find_fun_str(call_id, fun_r);
    if (NULL == fun){
      printf("Function %s has not been defined\n", call_id);
      return 1;
    }
    if (get_child_num(node) != fun->arity){
      printf("Wrong number of arguments of function %s\n", call_id);
      return 1;
    }

    struct node_var_str* args = fun->args;
    for (int i = 1; i <= fun->arity; i++){
      if ((is_fla (get_child(node, i)) && args->type == INTID) ||
          (is_term (get_child(node, i)) && args->type == BOOLID)){
        printf("Does not type check: argument %s vs type of %s\n", get_child(node, i)->token, args->name);
        return 1;
      }
      args = args->next;
    }
  }

  return 0;
}

// AST -> CFG translation

int tmp;
int cur_num = 0;

struct node_int* bb_beg_root = NULL;
struct node_int* bb_beg_tail = NULL;

struct node_int* bb_num_root = NULL;
struct node_int* bb_num_tail = NULL;

struct br_instr* bb_root = NULL;
struct br_instr* bb_tail = NULL;

struct asgn_instr* assgn_tmp_root = NULL;
struct asgn_instr* assgn_tmp_tail = NULL;

struct asgn_instr* asgn_root = NULL;
struct asgn_instr* asgn_tail = NULL;

struct br_instr* br_instrs = NULL;

void proc_rec(struct ast* node) {
  
  if (node->ntoken == IF) {
    // encoding of the guard

    int tmp_num1 = cur_num + 1;
    int tmp_num2 = cur_num + 2;
    int tmp_num3 = cur_num + 3;
    cur_num += 4;

    struct br_instr* bri = mk_cbr(bb_num_tail->id, 999, tmp_num1, tmp_num2);
    push_br(bri, &bb_root, &bb_tail);

    push_int(get_child(node, 1)->next->id, &bb_beg_root, &bb_beg_tail);
    push_int(tmp_num1, &bb_num_root, &bb_num_tail);

    proc_rec(get_child(node, 2));
    bri = mk_ubr(bb_num_tail->id, tmp_num3);
    push_br(bri, &bb_root, &bb_tail);

    push_int(get_child(node, 2)->next->id, &bb_beg_root, &bb_beg_tail);
    push_int(tmp_num2, &bb_num_root, &bb_num_tail);

    proc_rec(get_child(node, 3));

    bri = mk_ubr(bb_num_tail->id, tmp_num3);
    push_br(bri, &bb_root, &bb_tail);

    push_int(tmp_num3, &bb_num_root, &bb_num_tail);

    struct asgn_instr* asgn = mk_uasgn(tmp_num3, node->parent->id, node->id, REGID);
    push_asgn(asgn, &assgn_tmp_root, &assgn_tmp_tail);
    push_int(get_child(node, 3)->next->id, &bb_beg_root, &bb_beg_tail);
  }
  else if (node -> child != NULL){
    struct ast_child* temp_ast_child_root = node -> child;
    while(temp_ast_child_root != NULL){
      proc_rec(temp_ast_child_root -> id);
      temp_ast_child_root = temp_ast_child_root -> next;
    }
  }
}

int compute_br_structure(struct ast* node){
  if (node->ntoken == FUNID) {
    push_istr(cur_num, node->token, &ifun_r, &ifun_t);
    push_int(cur_num, &bb_num_root, &bb_num_tail);
    push_int(node->id, &bb_beg_root, &bb_beg_tail);
    proc_rec(get_child(node->parent, get_child_num(node->parent)));
    struct br_instr* bri = mk_ubr(bb_num_tail->id, -1);
    push_br(bri, &bb_root, &bb_tail);
    cur_num++;
  }
  return 0;
}

int inp_counter;

int fill_instrs (struct ast* node) {

  if (bb_beg_root != NULL && bb_beg_root->id == node->id){
    if (br_instrs == NULL){
      br_instrs = bb_root;
    }
    dequeue_int(&bb_beg_root);
  }

  if (node->ntoken == FUNID) {
    inp_counter = 1;
  }
  else if (node->ntoken == DECLID){    // var inputs
    struct asgn_instr* asgn = mk_uasgn(br_instrs->id, node->id, -inp_counter, INP);
    push_asgn(asgn, &asgn_root, &asgn_tail);
    inp_counter++;
  } else if (node->ntoken == CALL){
    for (int i = 1; i <= get_child_num(node); i++) {
      struct asgn_instr* asgn = mk_uasgn(br_instrs->id, -i, get_child(node, i)->id, REGID);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    }
    if (node->parent->ntoken == IF &&
        (get_child(node->parent, 2) == node ||
         get_child(node->parent, 3) == node)) {
      struct asgn_instr* asgn = mk_casgn(br_instrs->id, node->parent->id, node->token);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    } else {
      struct asgn_instr* asgn = mk_casgn(br_instrs->id, node->id, node->token);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    }
  }
  else if (node->ntoken == CONST || node->ntoken == TRUE || node->ntoken == FALSE || node->ntoken == VARID || node->ntoken == LET){

    int val, type;
    if (node->ntoken == VARID) {val = find_var_str(node->id, node->token, vars_r)->begin_id; type = REGID;}
    else if (node->ntoken == LET) {val = get_child(node, 3)->id; type = REGID;}
    else if (node->ntoken == TRUE) {val = 1; type = CONST;}
    else if (node->ntoken == FALSE) {val = 0; type = CONST;}
    else {val = atoi(node->token); type = CONST;}

    if (node->parent->ntoken == IF &&
        (get_child(node->parent, 2) == node ||
         get_child(node->parent, 3) == node)){
      struct asgn_instr* asgn = mk_uasgn(br_instrs->id, node->parent->id, val, type);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    } else{
      struct asgn_instr* asgn = mk_uasgn(br_instrs->id, node->id, val, type);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    }
  }
  else if (node->ntoken == EQ || node->ntoken == LT || node->ntoken == LE || node->ntoken == GT ||
           node->ntoken == GE || node->ntoken == DIV || node->ntoken == MOD ||
           node->ntoken == MINUS || node->ntoken == MULT || node->ntoken == OR ||
           node->ntoken == AND || node->ntoken == MULT || node->ntoken == PLUS){
    struct asgn_instr* asgn;
    int lhs;
    int op1 = get_child(node, 1)->id;
    int op2 = get_child(node, 2)->id;

    if (node->parent->ntoken == IF &&
        (get_child(node->parent, 2) == node ||
         get_child(node->parent, 3) == node)){
      lhs = node->parent->id;
    } else {
      lhs = node->id;
    }

    asgn = mk_basgn(br_instrs->id, lhs, op1, op2, node->ntoken);
    push_asgn(asgn, &asgn_root, &asgn_tail);

    // additional ops
    if (node->ntoken == OR || node->ntoken == AND || node->ntoken == MULT || node->ntoken == PLUS){
      for (int i = 3; i <= get_child_num(node); i++) {
        struct asgn_instr* asgn;
        int op2 = get_child(node, i)->id;
        
        asgn = mk_basgn(br_instrs->id, lhs, lhs, op2, node->ntoken);
        push_asgn(asgn, &asgn_root, &asgn_tail);
      }
    }
  }
  else if (node->ntoken == NOT){
    struct asgn_instr* asgn;
    int lhs;
    int op1 = get_child(node, 1)->id;

    if (node->parent->ntoken == IF &&
        (get_child(node->parent, 2) == node ||
         get_child(node->parent, 3) == node)){
          lhs = node->parent->id;
        } else {
      lhs = node->id;
    }

    asgn = mk_uasgn(br_instrs->id, lhs, op1, NOT);
    push_asgn(asgn, &asgn_root, &asgn_tail);
  }

  if (is_term(node) || is_fla(node)) {
    struct ast* parent_node = node->parent;
    if (parent_node->ntoken == LET && get_child(parent_node, 2) == node) {
      struct asgn_instr* asgn = mk_uasgn(br_instrs->id, get_child(parent_node, 1)->id, node->id, REGID);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    }
    else if (parent_node->ntoken == DEFFUN && node->ntoken != DECLID) {
      struct asgn_instr* asgn = mk_uasgn(br_instrs->id, 0, node->id, REGID);
      push_asgn(asgn, &asgn_root, &asgn_tail);
    }
    else if (parent_node->ntoken == PRINT || parent_node->ntoken == EVAL) {
      struct asgn_instr* asgn = mk_uasgn(br_instrs->id, -1, get_child(parent_node, 2)->id, REGID);
      push_asgn(asgn, &asgn_root, &asgn_tail);
      asgn = mk_casgn(br_instrs->id, parent_node->id, "print");
      push_asgn(asgn, &asgn_root, &asgn_tail);
    }
  }
  
  if (bb_beg_root != NULL && node->next != NULL && bb_beg_root->id == node->next->id){
    if (br_instrs->cond == 0){
      struct asgn_instr* tmp = assgn_tmp_root;
      while (tmp != NULL){
        if (tmp->bb == br_instrs->id && asgn_tail->bb != br_instrs->id){
          struct asgn_instr* copy = mk_asgn(tmp->bb, tmp->lhs, tmp->bin, tmp->op1, tmp->op2, tmp->type);
          push_asgn(copy, &asgn_root, &asgn_tail);
        }
        tmp = tmp->next;
      }
    } else {
      br_instrs->cond = node->id;
    }
    br_instrs = br_instrs->next;
    dequeue_int(&bb_beg_root);
  }
  
  return 0;
}

void print_interm() {
  printf ("\nfunction %s\n\n", find_istr(ifun_r, bb_root->id));
  printf ("entry:\n");

  struct asgn_instr* asgn = asgn_root;
  struct br_instr* br = bb_root;

  while (asgn != NULL){
    if (asgn->bb != br->id){
      if (br->cond == 0){
        if (br->succ1 != -1)
          printf("br bb%d\n\n", br->succ1);
        else
          printf("br exit\n\n");
      }
      else printf("br v%d bb%d bb%d\n\n", br->cond, br->succ1, br->succ2);
      br = br->next;
      if (br == NULL) return;
      char* fun_name = find_istr(ifun_r, br->id);
      if (fun_name != NULL){
        printf ("\nfunction %s\n\n", fun_name);
        printf ("bb entry:\n");
      } else {
        printf ("bb%d:\n", br->id);
      }
    }
    if (asgn->bin == 0){
      if (asgn->type == CONST)                                            //for constant
        printf("v%d := %d\n", asgn->lhs, asgn->op1);
      else if (asgn->type == NOT)
        printf("v%d := not v%d\n", asgn->lhs, asgn->op1);
      else if (asgn->type == INP)
        printf("v%d := a%d\n", asgn->lhs, -asgn->op1);
      else if (asgn->lhs == 0)
        printf("rv := v%d\n", asgn->op1);
      else if (asgn->lhs < 0)
        printf("a%d := v%d\n", -asgn->lhs, asgn->op1);
      else
        printf("v%d := v%d\n", asgn->lhs, asgn->op1);

    }
    else if (asgn->bin == 1){
      if (asgn->type == EQ)
        printf("v%d := v%d = v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == LT)
        printf("v%d := v%d < v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == PLUS)
        printf("v%d := v%d + v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == MINUS)
        printf("v%d := v%d - v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == AND)
        printf("v%d := v%d and v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == LE)
        printf("v%d := v%d <= v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == MULT)
        printf("v%d := v%d * v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == DIV)
        printf("v%d := v%d div v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == MOD)
        printf("v%d := v%d mod v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == GT)
        printf("v%d := v%d > v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == GE)
        printf("v%d := v%d >= v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      else if (asgn->type == OR)
        printf("v%d := v%d or v%d\n", asgn->lhs, asgn->op1, asgn->op2);
    }
    else if (asgn->bin == 2){
      printf("call %s \n", asgn->fun);
      if (strcmp(asgn->fun, "print") != 0)
        printf("v%d := rv\n", asgn->lhs);
    }
    asgn = asgn->next;
  }
  printf("br exit\n");
}

char val_1[10]= "--cp";           //for cp
char val_2[10] = "--cse";        //for the cse
int result, result_2;            // int to store the value


//---------------------------------------------------constant propogation function-----------------------------------------------------------//







struct  Node
{
  int lhs;
 
  int rhs;
                                  //count made
  struct  Node *next;
  
};

void linkedlisttraversal(struct Node *ptr)
{   while(ptr!=NULL)
  {
  printf("The value of lhs is %d\n ",ptr->lhs);
  //
  printf("the value of op1 is %d\n ", ptr->rhs);
  
  ptr= ptr->next;
    }
}
                                                             
struct Node *insertatend(struct Node *head, int data_1, int data_2 )             //  lhs, rhs , count
{

  struct Node *ptr = (struct Node*)malloc(sizeof(struct Node));
  struct Node  *p= head;
  
  ptr->lhs = data_1;

  ptr->rhs= data_2;

 
  while(p->next!= NULL)
  {
    p = p->next;

  }
  p->next = ptr;
  ptr->next = NULL;
  return head;

}

//----------------------------------------------------check if the values exit in the block------------------------------------------//

int check(struct Node *ptr, int data_1, int data_2)                      //data_1 = lhs, data_2 = op1
{
  
   while(ptr!= NULL)

   {
    
    //for common expression---------------------------------------------------------of constants
   
   if (ptr->lhs== data_1 &&  ptr->rhs == data_2)                //found return 0

   {
     //printf("check_1 triggered\n");   
     return 0;

                                      //the check part triggered

   }

   //for the checking of operation part-----------------------------------------------------------------
  
   else if  (NULL )

   {




   }
 ptr->next = ptr;

   //printf("check_2 triggered\n");
  return 1;


}



}






 int count =0;

int temp_str;                             //variable def
void const_prop()                      
{
  printf("under the cp function");                      //added
  printf ("\nfunction %s\n\n", find_istr(ifun_r, bb_root->id));
  printf ("entry:\n"); 
  //printf("check_2\n");                 //checking part

  //-----------------------------------------------------------logic for the constatnt propogation------------------------------------------------//

  struct asgn_instr* asgn = asgn_root;
  struct br_instr* br = bb_root;
  
  
      
  while (asgn != NULL){  

              //-----------------------------------------------------------------------------------//
    if (asgn->bb != br->id){
     // printf("the value of bb is %d\n",asgn->bb);       //checking
      //printf("the value of id is %d\n",br->id);         //checking


      if (br->cond == 0){
        if (br->succ1 != -1){
          printf("br bb%d\n\n", br->succ1);

        }
        else{
          printf("br exit\n\n");
        }
      }
      else printf("br v%d bb%d bb%d\n\n", br->cond, br->succ1, br->succ2);
      br = br->next;
      if (br == NULL) return;
      char* fun_name = find_istr(ifun_r, br->id);
      if (fun_name != NULL){
        printf ("\nfunction %s\n\n", fun_name);
        printf ("bb entry:\n");
      } else {
        printf ("bb%d:\n", br->id);
      }
    }

  //---------------------------------------------------------------oper----------------------------------------------------------------//
     
    // int prev_value = temp_str;
     //printf("previous_value %d\n", prev_value);
    if (asgn->bin == 0){
       
      if (asgn->type == CONST)                   
          
         {                                           //insertion
          struct Node *head;
          head= (struct Node*)malloc(sizeof(struct Node));
          
          insertatend(head,asgn->lhs,asgn->op1);
          //linkedlisttraversal(head);                       //traversal

          

         ///////////////////////////////////////////////////////////////
         if (check(head, asgn->lhs,asgn->op1) == 1) {


         // printf("the value of temp is %d\n", temp_str);                              //check
        //printf("the prev_value %d and temp value %d", prev_value, temp_str);
        //if (prev_value == temp_str)
        //{   
                                           //for constant
        printf("v%d := %d\n", asgn->lhs, asgn->op1); 
        temp_str = asgn->op1;              //constant assignmnet for the block
        //temp_str = as;
      
       
          }

     ////////////////////////////////////////////////////////     
        }  
      
      else if (asgn->type == NOT)
      {
        printf("v%d := not v%d\n", asgn->lhs, asgn->op1);
      }
      else if (asgn->type == INP)
      {  
        printf("v%d := a%d\n", asgn->lhs, -asgn->op1);      //---------------assignment------------------------------//
        //printf("check_in_inp\n");                              //added
      }
      else if (asgn->lhs == 0)
      {
        printf("rv := v%d\n", asgn->op1);
      }
      else if (asgn->lhs < 0)
      {
        printf("a%d := v%d\n",-asgn->lhs, asgn->op1);
        //printf("inside\n");                                                       //check
      }
      else{
        printf("v%d := v%d\n", asgn->lhs, asgn->op1);            //registor assignment for the block

        //printf("check for register\n");                             //check
      }

    }
    else if (asgn->bin == 1){
      //int temp_str;                                                                    //check--------------------------------
      if (asgn->type == EQ){
        printf("v%d := v%d = v%d\n", asgn->lhs, asgn->op1, asgn->op2);

      }
      else if (asgn->type == LT){
        printf("v%d := v%d < v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == PLUS){

        /////////////////////////////////////////

        
        
        
        //printf("%d\n", asgn->lhs);
        printf("v%d := v%d + v%d\n", asgn->lhs, asgn->op1, asgn->op2);
        
        //printf("in the addition\n");


       
      
      ////////////////////////////////////////////                   
      

      }
      else if (asgn->type == MINUS){
        printf("v%d := v%d - v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == AND){
        printf("v%d := v%d and v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == LE) {
        printf("v%d := v%d <= v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == MULT){
        printf("v%d := v%d * v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == DIV) {
        printf("v%d := v%d div v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == MOD) {
        printf("v%d := v%d mod v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == GT) {
        printf("v%d := v%d > v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == GE) {
        printf("v%d := v%d >= v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == OR){
        printf("v%d := v%d or v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
    }

    //---------------------------------------------------------------------------------------------------------------------------------------------//

    else if (asgn->bin == 2){
      printf("call %s \n", asgn->fun);
      if (strcmp(asgn->fun, "print") != 0)
        printf("v%d := rv\n", asgn->lhs);
    }
    asgn = asgn->next;
  }
  printf("br exit\n");

  
    
   
}

//-----------------------------------------------------common subexpression elimination function------------------------------------------------------------//


void cse_fun()
{
   
   printf("under the cse function\n");
    printf ("\nfunction %s\n\n", find_istr(ifun_r, bb_root->id));
  printf ("entry:\n"); 
  //printf("check_2\n");                 //checking part

  //-----------------------------------------------------------logic for the constatnt propogation------------------------------------------------//

  struct asgn_instr* asgn = asgn_root;
  struct br_instr* br = bb_root;
 
  while (asgn != NULL){
    if (asgn->bb != br->id){
      if (br->cond == 0){
        if (br->succ1 != -1){
          printf("br bb%d\n\n", br->succ1);
        }
        else{
          printf("br exit\n\n");
        }
      }
      else printf("br v%d bb%d bb%d\n\n", br->cond, br->succ1, br->succ2);
      br = br->next;
      if (br == NULL) return;
      char* fun_name = find_istr(ifun_r, br->id);
      if (fun_name != NULL){
        printf ("\nfunction %s\n\n", fun_name);
        printf ("bb entry:\n");
      } else {
        printf ("bb%d:\n", br->id);
      }
    }

  //---------------------------------------------------------------oper----------------------------------------------------------------//

    if (asgn->bin == 0){
      int temp_str;  
      if (asgn->type == CONST)   
          
         {                                     //for constant
        printf("v%d := %d check_3\n ", asgn->lhs, asgn->op1);}  //checking part
      
      else if (asgn->type == NOT)
      {
        printf("v%d := not v%d\n", asgn->lhs, asgn->op1);
      }
      else if (asgn->type == INP)
      {
        printf("v%d := a%d\n", asgn->lhs, -asgn->op1);
      }
      else if (asgn->lhs == 0)
      {
        printf("rv := v%d\n", asgn->op1);
      }
      else if (asgn->lhs < 0)
      {
        printf("a%d := v%d\n", -asgn->lhs, asgn->op1);
      }
      else{
        printf("v%d := v%d\n", asgn->lhs, asgn->op1);
      }

    }
    else if (asgn->bin == 1){
      if (asgn->type == EQ){
        printf("v%d := v%d = v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == LT){
        printf("v%d := v%d < v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == PLUS){
        printf("v%d := v%d + v%d check_4\n ", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == MINUS){
        printf("v%d := v%d - v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == AND){
        printf("v%d := v%d and v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == LE) {
        printf("v%d := v%d <= v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == MULT){
        printf("v%d := v%d * v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == DIV) {
        printf("v%d := v%d div v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == MOD) {
        printf("v%d := v%d mod v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == GT) {
        printf("v%d := v%d > v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == GE) {
        printf("v%d := v%d >= v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
      else if (asgn->type == OR){
        printf("v%d := v%d or v%d\n", asgn->lhs, asgn->op1, asgn->op2);
      }
    }

    //---------------------------------------------------------------------------------------------------------------------------------------------//

    else if (asgn->bin == 2){
      printf("call %s \n", asgn->fun);
      if (strcmp(asgn->fun, "print") != 0)
        printf("v%d := rv\n", asgn->lhs);
    }
    asgn = asgn->next;
  }
  printf("br exit\n");

   
}





int main (int argc, char *argv[10]) {
  int retval = yyparse();

  push_fun_str("GET-INT", INT, 0, NULL, &fun_r, &fun_t);
  push_fun_str("GET-BOOL", BOOL, 0, NULL, &fun_r, &fun_t);
  if (retval == 0) retval = visit_ast(get_fun_var_types);
  if (retval == 0) retval = visit_ast(type_check);
  if (retval == 0) print_ast();      // run "dot -Tpdf ast.dot -o ast.pdf" to create a PDF
  else {
    clean_fun_str(&fun_r);
    clean_var_str(&vars_r);
    return 1;
  }

  visit_ast(compute_br_structure);
  visit_ast(fill_instrs);

  //print_interm();
  
  // condition trigers only if other argumnents are provided  
  if (argc == 1)
  {
    print_interm();

  }



  if (argc >1)
  {

  result = strcmp(argv[1], val_1);
  result_2 = strcmp(argv[1], val_2);

  if (result == 0 && argc >1)
  {
                                    
                              //struct asgn_instr* ptr;
    const_prop();  //function calling
  }

  else if (result_2 ==0 && argc >1) {

    cse_fun();       //function calling
  }
  
  }

  clean_asgns(&asgn_root);
  clean_asgns(&assgn_tmp_root);
  clean_bbs(&bb_root);
  clean_istr(&ifun_r);
  clean_fun_str(&fun_r);
  clean_var_str(&vars_r);
 

  

  free_ast();
  return retval;
}

