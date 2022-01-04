#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "structures.h"

#define PORT 8000
#define TRAIN "./IRCTC/Train"
#define BOOKING "./IRCTC/Booking"
#define USER "./IRCTC/User"



//------------------Funtion declaration--------------------------------
void service_cli(int sock);
void login(int client_sock);
void signup(int client_sock);
int menu(int client_sock,int type,int id);
void crud_train(int client_sock);
void crud_user(int client_sock);
int user_function(int client_sock,int choice,int type,int id);



//everything is performed by child process

//-------------------- Service every client -----------------------------

void service_cli(int sock){
	int choice;
	printf("\n\tClient Connected\n");
	do{
		read(sock, &choice, sizeof(int));		
		if(choice==1)
			login(sock);
		if(choice==2)
			signup(sock);
		if(choice==3)			// exit
			break;
	}while(1);

	close(sock);
	printf("\n\tClient Disconnected\n");
}

//-------------------- Login function -----------------------------

//Normal user will be able to unlock only after accessing the menu
//Admin and agent unlocks before accessing the menu, therefore multiple agents can concurrently access the menu(which is the critical section)
//to have multiple reads, we dont use sepmaphores but use file locking instead

void login(int client_sock)
{
	int fd_user = open(USER,O_RDWR);
	int id,type,valid=0,user_valid=0;
	char password[50];
	struct user u;

	//read id and password sent by client
	read(client_sock,&id,sizeof(id));
	read(client_sock,&password,sizeof(password));

	//RECORD LOCKING MECHANISM..............
	struct flock lock;

	lock.l_start = (id-1)*sizeof(struct user);
	lock.l_len = sizeof(struct user);
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();

	lock.l_type = F_WRLCK; //write lock applied
	fcntl(fd_user,F_SETLKW, &lock); //fd_user= file descriptor to the database

	//CHECKING BOTH USERID AND PASSWORD IN THE DATABASE
	while(read(fd_user,&u,sizeof(u)))
	{
		if(u.login_id==id)
		{
			user_valid=1;
								//given a ID, check if password matches
			if(strncmp(password,u.password,strlen(password)) == 0)
			{
				valid = 1;
				type = u.type;
				break;
			}
			
			else
			{
				valid = 0;
				break;
			}
		}
		else
		{
			user_valid = 0;
			valid=0;
		}
	}

	// same agent is allowed from multiple terminals..
	// so unlocking his user record just after checking his credentials and allowing further
	
	
	//admin and agent
	if(type!=2)
	{
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}

	// if valid user, show him menu
	if(user_valid)
	{
		write(client_sock,&valid,sizeof(valid));
		if(valid)
		{
			//write the type of user in client_socket, so client side identifies which menu to show[admin or customer/agent]
			write(client_sock,&type,sizeof(type));
			while(menu(client_sock,type,id)!=-1);
		}
	}
	
	else
		write(client_sock,&valid,sizeof(valid)); 	//pass the valid=0 value in socket to tell client its invalid

	// same user is not allowed from multiple terminals..
	// so unlocking his user record after he logs out only to not allow him from other terminal

	//customer
	if(type==2)
	{
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}
} 

//-------------------- Signup function -----------------------------

//creating a user record[LOCKING USED]

void signup(int client_sock){

	int fd_user = open(USER,O_RDWR);
	int type,id=0;
	char name[50],password[50];
	struct user u,temp;
	
	//client enters the type,name and password
	read(client_sock, &type, sizeof(type));
	read(client_sock, &name, sizeof(name));
	read(client_sock, &password, sizeof(password));
	
	//reached at the end of the file, where we will insert the record, and lock it
	int fp = lseek(fd_user, 0, SEEK_END);

	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_start = fp;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();


	fcntl(fd_user,F_SETLKW, &lock);

	// if file is empty, login id will start from 1
	// else it will increment from the previous value
	
	//file is empty
	if(fp==0){
		u.login_id = 1;
		//put all info in user structure (creating a record)
		strcpy(u.name, name);
		strcpy(u.password, password);
		u.type=type;
		write(fd_user, &u, sizeof(u));
		write(client_sock, &u.login_id, sizeof(u.login_id)); //if file is empty, put loginid as 1 in socket and pass it to client
	}
	
	else{
		fp = lseek(fd_user, -1 * sizeof(struct user), SEEK_END);
		read(fd_user, &u, sizeof(u)); //read the last user's ID from database, therefore lseek
		u.login_id++; //increment the last read user's ID
		//now read work over
		
		//creating a user record and updating in database
		strcpy(u.name, name);
		strcpy(u.password, password);
		u.type=type;
		write(fd_user, &u, sizeof(u));
		//put loginid in socket and pass it to client
		write(client_sock, &u.login_id, sizeof(u.login_id));
	}
	
	lock.l_type = F_UNLCK;
	fcntl(fd_user, F_SETLK, &lock);

	close(fd_user);
	
}

//-------------------- Main menu function -----------------------------

int menu(int client_sock,int type,int id){
	int choice,ret;

	// for ADMIN
	if(type==0){
		read(client_sock,&choice,sizeof(choice));
		if(choice==1){					// CRUD options on train
			crud_train(client_sock);
			return menu(client_sock,type,id);	
		}
		else if(choice==2){				// CRUD options on User
			crud_user(client_sock);
			return menu(client_sock,type,id);
		}
		else if (choice ==3)				// Logout
			return -1;
	}
	else if(type==2 || type==1){				// For agent and customer
		read(client_sock,&choice,sizeof(choice));
		ret = user_function(client_sock,choice,type,id);
		if(ret!=5)
			return menu(client_sock,type,id);
		else if(ret==5)
			return -1;
	}		
}

//---------------------- CRUD operation on train --------------------

void crud_train(int client_sock){
	int valid=0;	
	int choice;
	//take the choice on what crud operations to be performed on train from client
	read(client_sock,&choice,sizeof(choice));
	
	//Same process as that of adding user
	if(choice==1){  					// Add train  	
		char tname[50];
		int tid = 0;
		//admin client sends only the Train-name
		read(client_sock,&tname,sizeof(tname));
		
		struct train tdb,temp;
		struct flock lock;
		int fd_train = open(TRAIN, O_RDWR); //open train database and return fd_train
		
		//creating the structure train tdb which is to be written on the database
		tdb.train_number = tid;
		strcpy(tdb.train_name,tname);
		tdb.total_seats = 50;				// by default, we are taking 50 seats
		tdb.available_seats = 50;
		
		//put the pointer to last and LOCK IT
		int fp = lseek(fd_train, 0, SEEK_END); 

		lock.l_type = F_WRLCK;
		lock.l_start = fp;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd_train, F_SETLKW, &lock);
		
		//empty train database
		if(fp == 0){
			valid = 1;
			//insert the record in train database(ID=0)
			write(fd_train, &tdb, sizeof(tdb));
			lock.l_type = F_UNLCK;
			fcntl(fd_train, F_SETLK, &lock);
			close(fd_train);
			write(client_sock, &valid, sizeof(valid)); //return valid to client
		}
		else{
			valid = 1;
			//go to the last train record, pick the train ID, increment it and then push the newly created record at last
			lseek(fd_train, -1 * sizeof(struct train), SEEK_END);
			read(fd_train, &temp, sizeof(temp));
			tdb.train_number = temp.train_number + 1;
			write(fd_train, &tdb, sizeof(tdb));
			write(client_sock, &valid,sizeof(valid)); //return valid to client
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);
		
	}
	
	//Read lock is being applied instead of semaphores so that multiple readers can concurrently access the database(RDONLY mode)
	else if(choice==2){					// View/ Read trains
		struct flock lock;
		struct train tdb;
		int fd_train = open(TRAIN, O_RDONLY);
		
		//Read locking mechanism[MANDATORY LOCKING], cus we are viewing the whole database
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lock);
		
		//go to last to get the size of the file
		int fp = lseek(fd_train, 0, SEEK_END);
		int no_of_trains = fp / sizeof(struct train);
		write(client_sock, &no_of_trains, sizeof(int)); //send the no of trains into socket and send to client

		lseek(fd_train,0,SEEK_SET); //come to first
		
		//till you reach the EOF of the train database
		while(fp != lseek(fd_train,0,SEEK_CUR)){
			read(fd_train,&tdb,sizeof(tdb));
			//push the information into the socket and pass to the client
			write(client_sock,&tdb.train_number,sizeof(int));
			write(client_sock,&tdb.train_name,sizeof(tdb.train_name));
			write(client_sock,&tdb.total_seats,sizeof(int));
			write(client_sock,&tdb.available_seats,sizeof(int));
		}
		valid = 1;
		//when the last reader process is unlocking, it gives way to the writer process to perform updates into the train database
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);
	}

	else if(choice==3){					// Update train
		crud_train(client_sock);
		
		int choice,valid=0,tid;
		struct flock lock;
		struct train tdb;
		int fd_train = open(TRAIN, O_RDWR);
		
		//client sends the server: tid and server reads it into tid
		read(client_sock,&tid,sizeof(tid));
		
		//LOCKING MECHANISM
		//tid starting from 0
		lock.l_type = F_WRLCK;
		lock.l_start = (tid)*sizeof(struct train);
		lock.l_len = sizeof(struct train);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lock);
		
		//make the read write pointer point to start and then go to the desired record 
		lseek(fd_train, 0, SEEK_SET);
		lseek(fd_train, (tid)*sizeof(struct train), SEEK_CUR); //starting of the desired record
		
		read(fd_train, &tdb, sizeof(struct train)); //reaches to the end of the desired record
		
		//take choice from client, what he wants to update inside the train record
		read(client_sock,&choice,sizeof(int));
		
		if(choice==1){							// update train name
			//pass the old train name to the client_sock and pass that to client
			//then take the new train name in client_sock and store that in tdb.train_name
			write(client_sock,&tdb.train_name,sizeof(tdb.train_name));
			read(client_sock,&tdb.train_name,sizeof(tdb.train_name));
			
		}
		else if(choice==2){						// update total number of seats
		
			int seats = tdb.total_seats;
			write(client_sock,&tdb.total_seats,sizeof(tdb.total_seats));
			read(client_sock,&tdb.total_seats,sizeof(tdb.total_seats));
			tdb.available_seats += tdb.total_seats - seats;
		}
	
		lseek(fd_train, -1*sizeof(struct train), SEEK_CUR); //goes to the starting of the desired record, from line no 415
		write(fd_train, &tdb, sizeof(struct train)); //update into the database
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);	
	}

	else if(choice==4){						// Delete train
		crud_train(client_sock);
		struct flock lock;
		struct train tdb;
		int fd_train = open(TRAIN, O_RDWR);
		int tid,valid=0;
		
		//get tid from client
		read(client_sock,&tid,sizeof(tid));
		
		//record locking on the desired record given tid(tid starts from 0)
		lock.l_type = F_WRLCK;
		lock.l_start = (tid)*sizeof(struct train);
		lock.l_len = sizeof(struct train);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lock);
		
		lseek(fd_train, 0, SEEK_SET);
		lseek(fd_train, (tid)*sizeof(struct train), SEEK_CUR); //goes to beginning of record
		read(fd_train, &tdb, sizeof(struct train)); //read the desird record and goes to end of record
		strcpy(tdb.train_name,"deleted");
		lseek(fd_train, -1*sizeof(struct train), SEEK_CUR); //goes to the starting of the record
		write(fd_train, &tdb, sizeof(struct train)); //update the record with the new record with name: "DELETED"
		
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lock);
		close(fd_train);	
	}	
}

//---------------------- CRUD operation on user --------------------
void crud_user(int client_sock){
	int valid=0;	
	int choice;
	read(client_sock,&choice,sizeof(choice));
	if(choice==1){    					// Add user
		char name[50],password[50];
		int type;
		read(client_sock, &type, sizeof(type));
		read(client_sock, &name, sizeof(name));
		read(client_sock, &password, sizeof(password));
		
		struct user udb;
		struct flock lock;
		int fd_user = open(USER, O_RDWR);
		int fp = lseek(fd_user, 0, SEEK_END);
		
		lock.l_type = F_WRLCK;
		lock.l_start = fp;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd_user, F_SETLKW, &lock);

		if(fp==0){
			udb.login_id = 1;
			strcpy(udb.name, name);
			strcpy(udb.password, password);
			udb.type=type;
			write(fd_user, &udb, sizeof(udb));
			valid = 1;
			write(client_sock,&valid,sizeof(int));
			write(client_sock, &udb.login_id, sizeof(udb.login_id));
		}
		else{
			fp = lseek(fd_user, -1 * sizeof(struct user), SEEK_END);
			read(fd_user, &udb, sizeof(udb));
			udb.login_id++;
			strcpy(udb.name, name);
			strcpy(udb.password, password);
			udb.type=type;
			write(fd_user, &udb, sizeof(udb));
			valid = 1;
			write(client_sock,&valid,sizeof(int));
			write(client_sock, &udb.login_id, sizeof(udb.login_id));
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
		
	}

	else if(choice==2){					// View user list
		struct flock lock;
		struct user udb;
		int fd_user = open(USER, O_RDONLY);
		
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);
		int fp = lseek(fd_user, 0, SEEK_END);
		int no_of_users = fp / sizeof(struct user);
		no_of_users--;
		write(client_sock, &no_of_users, sizeof(int));

		lseek(fd_user,0,SEEK_SET);
		while(fp != lseek(fd_user,0,SEEK_CUR)){
			read(fd_user,&udb,sizeof(udb));
			if(udb.type!=0){
				write(client_sock,&udb.login_id,sizeof(int));
				write(client_sock,&udb.name,sizeof(udb.name));
				write(client_sock,&udb.type,sizeof(int));
			}
		}
		valid = 1;
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}

	else if(choice==3){					// Update user
		crud_user(client_sock);
		int choice,valid=0,uid;
		char pass[50];
		struct flock lock;
		struct user udb;
		int fd_user = open(USER, O_RDWR);

		read(client_sock,&uid,sizeof(uid));

		lock.l_type = F_WRLCK;
		lock.l_start =  (uid-1)*sizeof(struct user);
		lock.l_len = sizeof(struct user);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);

		lseek(fd_user, 0, SEEK_SET);
		lseek(fd_user, (uid-1)*sizeof(struct user), SEEK_CUR);
		read(fd_user, &udb, sizeof(struct user));
		
		read(client_sock,&choice,sizeof(int));
		if(choice==1){					// update name
			write(client_sock,&udb.name,sizeof(udb.name));
			read(client_sock,&udb.name,sizeof(udb.name));
			valid=1;
			write(client_sock,&valid,sizeof(valid));		
		}
		else if(choice==2){				// update password
			read(client_sock,&pass,sizeof(pass));
			if(!strcmp(udb.password,pass))
				valid = 1;
			write(client_sock,&valid,sizeof(valid));
			read(client_sock,&udb.password,sizeof(udb.password));
		}
	
		lseek(fd_user, -1*sizeof(struct user), SEEK_CUR);
		write(fd_user, &udb, sizeof(struct user));
		if(valid)
			write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);	
	}

	else if(choice==4){						// Delete any particular user
		crud_user(client_sock);
		struct flock lock;
		struct user udb;
		int fd_user = open(USER, O_RDWR);
		int uid,valid=0;

		read(client_sock,&uid,sizeof(uid));

		lock.l_type = F_WRLCK;
		lock.l_start =  (uid-1)*sizeof(struct user);
		lock.l_len = sizeof(struct user);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);
		
		lseek(fd_user, 0, SEEK_SET);
		lseek(fd_user, (uid-1)*sizeof(struct user), SEEK_CUR);
		read(fd_user, &udb, sizeof(struct user));
		strcpy(udb.name,"deleted");
		strcpy(udb.password,"");
		lseek(fd_user, -1*sizeof(struct user), SEEK_CUR);
		write(fd_user, &udb, sizeof(struct user));
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);	
	}
}


//---------------------- User functions -----------------------
int user_function(int client_sock,int choice,int type,int id){
	int valid=0;
	if(choice==1){						// book ticket
		crud_train(client_sock);
		struct flock lockt;
		struct flock lockb;
		struct train tdb;
		struct booking bdb;
		int fd_train = open(TRAIN, O_RDWR);
		int fd_book = open(BOOKING, O_RDWR);
		int tid,seats;
		read(client_sock,&tid,sizeof(tid));		
		
		// train database - record locking
		lockt.l_type = F_WRLCK;
		lockt.l_start = tid*sizeof(struct train);
		lockt.l_len = sizeof(struct train);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();
		
		// booking database - mandatory locking
		lockb.l_type = F_WRLCK;
		lockb.l_start = 0;
		lockb.l_len = 0;
		lockb.l_whence = SEEK_END;
		lockb.l_pid = getpid();
		
		fcntl(fd_train, F_SETLKW, &lockt);
		lseek(fd_train,tid*sizeof(struct train),SEEK_SET); 	// train tid starts from 0
		
		read(fd_train, &tdb, sizeof(tdb));
		read(client_sock,&seats,sizeof(seats));

		if(tdb.train_number==tid)
		{		
			if(tdb.available_seats>=seats){
				valid = 1;
				tdb.available_seats -= seats;
				fcntl(fd_book, F_SETLKW, &lockb);
				int fp = lseek(fd_book, 0, SEEK_END);
				
				if(fp > 0){
					lseek(fd_book, -1*sizeof(struct booking), SEEK_CUR);
					read(fd_book, &bdb, sizeof(struct booking));
					bdb.booking_id++;
				}
				else 
					bdb.booking_id = 0;

				bdb.type = type;
				bdb.uid = id;
				bdb.tid = tid;
				bdb.seats = seats;
				write(fd_book, &bdb, sizeof(struct booking));
				lockb.l_type = F_UNLCK;
				fcntl(fd_book, F_SETLK, &lockb);
			 	close(fd_book);
			}
		
		lseek(fd_train, -1*sizeof(struct train), SEEK_CUR);
		write(fd_train, &tdb, sizeof(tdb));
		}

		lockt.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lockt);
		close(fd_train);
		write(client_sock,&valid,sizeof(valid));
		return valid;		
	}
	
	else if(choice==2){							// View bookings
		struct flock lock;
		struct booking bdb;
		int fd_book = open(BOOKING, O_RDONLY);
		int no_of_bookings = 0;
	
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lock);
	
		while(read(fd_book,&bdb,sizeof(bdb))){
			if (bdb.uid==id)
				no_of_bookings++;
		}

		write(client_sock, &no_of_bookings, sizeof(int));
		lseek(fd_book,0,SEEK_SET);

		while(read(fd_book,&bdb,sizeof(bdb))){
			if(bdb.uid==id){
				write(client_sock,&bdb.booking_id,sizeof(int));
				write(client_sock,&bdb.tid,sizeof(int));
				write(client_sock,&bdb.seats,sizeof(int));
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lock);
		close(fd_book);
		return valid;
	}

	else if (choice==3){
		
		int uid_from_booking;
	
								// update booking
		int choice = 2,bid,val;
		user_function(client_sock,choice,type,id);
		struct booking bdb;
		struct train tdb;
		struct flock lockb;
		struct flock lockt;
		int fd_book = open(BOOKING, O_RDWR);
		int fd_train = open(TRAIN, O_RDWR);
		read(client_sock,&bid,sizeof(bid));

		lockb.l_type = F_WRLCK;
		lockb.l_start = bid*sizeof(struct booking);
		lockb.l_len = sizeof(struct booking);
		lockb.l_whence = SEEK_SET;
		lockb.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lockb);
		lseek(fd_book,bid*sizeof(struct booking),SEEK_SET);
		read(fd_book,&bdb,sizeof(bdb));
		lseek(fd_book,-1*sizeof(struct booking),SEEK_CUR);
		
		lockt.l_type = F_WRLCK;
		lockt.l_start = (bdb.tid)*sizeof(struct train);
		lockt.l_len = sizeof(struct train);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();

		fcntl(fd_train, F_SETLKW, &lockt);
		lseek(fd_train,(bdb.tid)*sizeof(struct train),SEEK_SET);
		read(fd_train,&tdb,sizeof(tdb));
		lseek(fd_train,-1*sizeof(struct train),SEEK_CUR);


// newly added

		uid_from_booking = bdb.uid;
		if(uid_from_booking != id)
		{
			lockt.l_type = F_UNLCK;
			fcntl(fd_train, F_SETLK, &lockt);
			close(fd_train);
		
			lockb.l_type = F_UNLCK;
			fcntl(fd_book, F_SETLK, &lockb);
			close(fd_book);	
			
			valid = 0;
			write(client_sock,&valid,sizeof(valid));
			
			return valid;		
		}		
		
		
		
		read(client_sock,&choice,sizeof(choice));
	
		if(choice==1){							// increase number of seats required of booking id
			read(client_sock,&val,sizeof(val));
			if(tdb.available_seats>=val){
				valid=1;
				tdb.available_seats -= val;
				bdb.seats += val;
			}
		}
		else if(choice==2){						// decrease number of seats required of booking id
			valid=1;
			read(client_sock,&val,sizeof(val));
			tdb.available_seats += val;
			bdb.seats -= val;	
		}
		
		write(fd_train,&tdb,sizeof(tdb));
		lockt.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lockt);
		close(fd_train);
		
		write(fd_book,&bdb,sizeof(bdb));
		lockb.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lockb);
		close(fd_book);
		
		write(client_sock,&valid,sizeof(valid));
		return valid;
	}
	else if(choice==4){							// Cancel an entire booking
	
		int uid_from_booking;
		
		
		int choice = 2,bid;
		user_function(client_sock,choice,type,id);
		struct booking bdb;
		struct train tdb;
		struct flock lockb;
		struct flock lockt;
		int fd_book = open(BOOKING, O_RDWR);
		int fd_train = open(TRAIN, O_RDWR);
		read(client_sock,&bid,sizeof(bid));

		lockb.l_type = F_WRLCK;
		lockb.l_start = bid*sizeof(struct booking);
		lockb.l_len = sizeof(struct booking);
		lockb.l_whence = SEEK_SET;
		lockb.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lockb);
		lseek(fd_book,bid*sizeof(struct booking),SEEK_SET);
		read(fd_book,&bdb,sizeof(bdb));
		lseek(fd_book,-1*sizeof(struct booking),SEEK_CUR);
		
		lockt.l_type = F_WRLCK;
		lockt.l_start = (bdb.tid)*sizeof(struct train);
		lockt.l_len = sizeof(struct train);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();

		fcntl(fd_train, F_SETLKW, &lockt);
		lseek(fd_train,(bdb.tid)*sizeof(struct train),SEEK_SET);
		read(fd_train,&tdb,sizeof(tdb));
		lseek(fd_train,-1*sizeof(struct train),SEEK_CUR);

// newly added
		uid_from_booking = bdb.uid;
		if(uid_from_booking != id)
		{
			lockt.l_type = F_UNLCK;
			fcntl(fd_train, F_SETLK, &lockt);
			close(fd_train);
		
			lockb.l_type = F_UNLCK;
			fcntl(fd_book, F_SETLK, &lockb);
			close(fd_book);	
			
			valid = 0;
			write(client_sock,&valid,sizeof(valid));
			
			return valid;		
		}		
		
		
		tdb.available_seats += bdb.seats;
		bdb.seats = 0;
		valid = 1;

		write(fd_train,&tdb,sizeof(tdb));
		lockt.l_type = F_UNLCK;
		fcntl(fd_train, F_SETLK, &lockt);
		close(fd_train);
		
		write(fd_book,&bdb,sizeof(bdb));
		lockb.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lockb);
		close(fd_book);
		
		write(client_sock,&valid,sizeof(valid));
		return valid;
		
	}
	else if(choice==5)										// Logout
		return 5;

}
