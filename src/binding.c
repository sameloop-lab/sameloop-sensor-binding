// SPDX-License-Identifier: MIT
#include "sl-common.h"
#include "model.h"
static afb_api_t self;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool stopping;
static atomic_int pending;
static double temperature = 32.4, measured = 32.4, heat = 0.18, last, sample_time;
static bool valid = true, paused, provider_ok;
static uint64_t sequence;
static json_object *snapshot_locked(void) {
    json_object *j = json_object_new_object();
    sl_double(j,"value",measured);
    sl_bool(j,"valid",valid && provider_ok);
    sl_bool(j,"paused",paused);
    sl_double(j,"monotonic_s",sample_time);
    json_object_object_add(j,"sequence",json_object_new_uint64(sequence));
    sl_double(j,"heat",heat);
    return j;
}
static void tick(int signal, void *arg) {
    (void)arg;
    if (signal || atomic_load(&stopping)) return;
    json_object *f = sl_call(self,"sameloop-fan","read",json_object_new_object());
    double duty = 0;
    bool ok = f && sl_number(f,"applied",0,100,&duty);
    json_object_put(f);
    pthread_mutex_lock(&lock);
    double now = sl_now();
    temperature = sl_clamp(thermal_step(temperature,duty,heat,fmax(0,now-last)),-40,150);
    last = now;
    if (!paused) {
        measured = temperature; sample_time = now; sequence++; provider_ok = ok;
    }
    pthread_mutex_unlock(&lock);
    if (!atomic_load(&stopping)) {
        int job = afb_job_post(100,0,tick,NULL,&self);
        atomic_store(&pending,job);
        if (job < 0) AFB_API_ERROR(self,"Cannot reschedule sensor: %d",job);
    }
}
static void read_sensor(afb_req_t req, unsigned n, afb_data_t const p[]) {
    if (!sl_empty(n,p)) { sl_error(req,-EINVAL,"read expects an empty object"); return; }
    pthread_mutex_lock(&lock);
    json_object *j = snapshot_locked();
    pthread_mutex_unlock(&lock);
    sl_reply(req,0,j);
}
/* Internal test interface; only exposed by the optional public API fault verb. */
static void fault(afb_req_t req, unsigned n, afb_data_t const p[]) {
    json_object *j = sl_args(n,p);
    if (!j || json_object_object_length(j) == 0) goto invalid;
    json_object_object_foreach(j,key,value) {
        double x;
        if (!strcmp(key,"valid") || !strcmp(key,"paused")) {
            if (!json_object_is_type(value,json_type_boolean)) goto invalid;
        } else if (!strcmp(key,"heat")) {
            if (!sl_number(j,key,0,2,&x)) goto invalid;
        } else if (!strcmp(key,"temperature")) {
            if (!sl_number(j,key,-40,150,&x)) goto invalid;
        } else goto invalid;
    }
    pthread_mutex_lock(&lock);
    json_object *v;
    if ((v=sl_get(j,"valid"))) valid=json_object_get_boolean(v);
    if ((v=sl_get(j,"paused"))) paused=json_object_get_boolean(v);
    if ((v=sl_get(j,"heat"))) heat=json_object_get_double(v);
    if ((v=sl_get(j,"temperature"))) temperature=json_object_get_double(v);
    json_object *out=snapshot_locked();
    pthread_mutex_unlock(&lock);
    json_object_put(j);
    sl_reply(req,0,out);
    return;
invalid:
    json_object_put(j);
    sl_error(req,-EINVAL,"fault accepts valid/paused booleans, heat 0..2, temperature -40..150");
}
static int ctl(afb_api_t api, afb_ctlid_t id, afb_ctlarg_t arg, void *data) {
    (void)arg; (void)data;
    if (id == afb_ctlid_Init) {
        self=api; last=sl_now();
        int job=afb_job_post(100,0,tick,NULL,&self);
        atomic_store(&pending,job);
        return job < 0 ? job : 0;
    }
    if (id == afb_ctlid_Exiting) {
        atomic_store(&stopping,true);
        int job=atomic_load(&pending);
        if(job > 0) afb_job_abort(job);
    }
    return 0;
}
static const afb_verb_t verbs[] = {
    {.verb="read", .callback=read_sensor, .info="Read timestamped simulated temperature"},
    {.verb="fault", .callback=fault, .info="Private simulation fault injection"},
    {0}
};
const afb_binding_t afbBindingV4 = {
    .api="sameloop-sensor", .verbs=verbs, .mainctl=ctl,
    .require_api="sameloop-fan", .noconcurrency=1,
    .info="SameLoop temperature provider and thermal simulator"
};
