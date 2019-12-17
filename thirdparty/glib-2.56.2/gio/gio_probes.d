provider gio {
	probe task__new(void*, void*, void*, void*, void*);
	probe task__set_task_data(void*, void*, void*);
	probe task__set_priority(void*, int);
	probe task__set_source_tag(void*, void*);
	probe task__before_return(void*, void*, void*, void*);
	probe task__propagate(void*, unsigned int);
	probe task__before_run_in_thread(void*, void*);
	probe task__after_run_in_thread(void*, unsigned int);
};
