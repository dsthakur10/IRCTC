
struct train{
		int train_number;
		char train_name[50];
		int total_seats;
		int available_seats;
};

struct user{
		int login_id;
		char password[50];
		char name[50];
		int type; 			//admin = 0 | agent[multi login] = 1 | customer = 2 
};

struct booking{
		int booking_id;
		int type;
		int uid;		// reference key
		int tid;		// refernece key
		int seats;
};


