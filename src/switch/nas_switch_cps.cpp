/*
 * Copyright (c) 2016 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */


#include "nas_switch_cps.h"
#include "nas_switch_mac.h"
#include "dell-base-switch-element.h"

#include "cps_class_map.h"
#include "cps_api_object_key.h"
#include "cps_api_operation.h"
#include "event_log.h"

#include "nas_ndi_switch.h"
#include "cps_api_events.h"
#include <unordered_map>
#include <vector>


static cps_api_key_t _sws_ent_key;
static const unsigned int nas_def_counter_refresh_interval = 5;

const npu_id_t * npu_list_from_switch(BASE_CMN_LOGICAL_SWITCH_ID_t sw) {
    //@TODO use the standard switch model
    static npu_id_t ids[] = { 0 } ;
    return ids;
}

size_t npu_list_from_switch_len(BASE_CMN_LOGICAL_SWITCH_ID_t sw) {
    //@TODO use the standard switch model
    return 1;
}

static size_t number_of_switches() {
    return 1;
}

static cps_api_object_t create_obj_on_list(cps_api_object_list_t lst, cps_api_attr_id_t obj_type,
        cps_api_qualifier_t qual = cps_api_qualifier_TARGET) {
    cps_api_object_t o = cps_api_object_list_create_obj_and_append(lst);
    if (o==NULL) return NULL;

    cps_api_key_from_attr_with_qual(cps_api_object_key(o),obj_type,qual);
    return o;
}

static bool _is_switch_obj_empty (cps_api_object_t obj)
{
    cps_api_object_it_t    it;
    cps_api_attr_id_t      attr_id;
    bool                   ret = true;


    if (obj == NULL) return true;

    cps_api_object_it_begin(obj, &it);

    while (cps_api_object_it_valid(&it)) {

        attr_id = cps_api_object_attr_id (it.attr);

        if ((attr_id != BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_ID)
                && (attr_id != CPS_API_OBJ_KEY_ATTRS)) {
            ret = false;
            break;
        }

        cps_api_object_it_next (&it);
    }

    return ret;
}

static void _fill_obj_for_switch(cps_api_object_t obj, cps_api_object_t filter,
                                 BASE_CMN_LOGICAL_SWITCH_ID_t sw) {
    bool is_empty = true;
    cps_api_object_it_t    it;
    cps_api_attr_id_t      attr;
    std::vector <uint32_t> attrlist;
    const npu_id_t * npus = npu_list_from_switch(sw);
    STD_ASSERT(npus!=NULL);

    cps_api_object_attr_add_u32(obj,BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_ID,sw);

    is_empty = _is_switch_obj_empty(filter);

    nas_ndi_switch_param_t param;

    if (is_empty == true) {

        ndi_switch_attr_list_get(attrlist);

    } else {
        cps_api_object_it_begin(filter, &it);

        while (cps_api_object_it_valid(&it)) {
            attr = cps_api_object_attr_id (it.attr);

            if (attr != BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_ID)
                attrlist.push_back(attr);

            cps_api_object_it_next (&it);
        }
    }

    std::vector <uint32_t>::iterator attr_it = attrlist.begin();


    while (attr_it != attrlist.end()) {

        switch (*attr_it) {

            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_MODE:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_LAG_HASH_ALGORITHM:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_BRIDGE_TABLE_SIZE:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_ECMP_HASH_ALGORITHM:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_MAX_ECMP_ENTRY_PER_GROUP:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_MAC_AGE_TIMER:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_ACL_TABLE_MIN_PRIORITY:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_ACL_TABLE_MAX_PRIORITY:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_ACL_ENTRY_MIN_PRIORITY:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_ACL_ENTRY_MAX_PRIORITY:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_MAX_MTU:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_NUM_UNICAST_QUEUES_PER_PORT:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_NUM_MULTICAST_QUEUES_PER_PORT:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_NUM_QUEUES_CPU_PORT:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_NUM_QUEUES_PER_PORT:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_TOTAL_BUFFER_SIZE:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_INGRESS_BUFFER_POOL_NUM:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_EGRESS_BUFFER_POOL_NUM:
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_COUNTER_REFRESH_INTERVAL:

                if (ndi_switch_get_attribute(*npus,
                            (BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_t)*attr_it, &param)==cps_api_ret_code_OK) {
                    cps_api_object_attr_add_u32(obj,*attr_it, param.u32);
                }
                break;
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_NPU_IDENTIFIERS:
                cps_api_object_attr_add_u32(obj, *attr_it,*npus);
                break;
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_DEFAULT_MAC_ADDRESS:
                if (nas_switch_mac_get(&param.mac) == STD_ERR_OK) {
                    cps_api_object_attr_add(obj, *attr_it, &param.mac, sizeof(param.mac));
                }
                break;
            case BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_TEMPERATURE:
                if (ndi_switch_get_attribute(*npus,
                            (BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_t) *attr_it, &param)==cps_api_ret_code_OK) {
                    cps_api_object_attr_add(obj, *attr_it,&param.s32,sizeof(param.s32));
                }
                break;
            default:
                break;
        }

        ++attr_it;
    }
}

static cps_api_return_code_t _switch_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);

    cps_api_object_attr_t _switch = cps_api_get_key_data(filt,BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_ID);

    size_t ix = 0;
    size_t mx = number_of_switches();
    for ( ; ix < mx ; ++ix ) {
        if (_switch!=NULL && cps_api_object_attr_data_u32(_switch)!=ix) continue;
        if (npu_list_from_switch_len((BASE_CMN_LOGICAL_SWITCH_ID_t)ix) ==0 ) continue;

        cps_api_object_t obj = create_obj_on_list(param->list,BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY);
        if(obj==NULL) return cps_api_ret_code_ERR;

        _fill_obj_for_switch(obj, filt, (BASE_CMN_LOGICAL_SWITCH_ID_t)ix);

    }

    return cps_api_ret_code_OK;
}

static cps_api_return_code_t _set_generic_u32(BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_t attr,
        npu_id_t npu, cps_api_object_t obj) {

    nas_ndi_switch_param_t param;
    cps_api_object_attr_t _attr = cps_api_object_attr_get(obj,attr);
    if (_attr==NULL) return cps_api_ret_code_ERR;
    param.u32 = cps_api_object_attr_data_u32(_attr);
    return ndi_switch_set_attribute(npu,attr,&param)==STD_ERR_OK ? cps_api_ret_code_OK : cps_api_ret_code_ERR;
}

static cps_api_return_code_t _set_generic_mac(BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_t attr,
        npu_id_t npu, cps_api_object_t obj) {

    nas_ndi_switch_param_t param;
    cps_api_object_attr_t _attr = cps_api_object_attr_get(obj,attr);
    if (_attr==NULL) return cps_api_ret_code_ERR;
    memcpy(&param.mac,cps_api_object_attr_data_bin(_attr),sizeof(param.mac));
    return ndi_switch_set_attribute(npu,attr,&param)==STD_ERR_OK ? cps_api_ret_code_OK : cps_api_ret_code_ERR;
}

static const std::unordered_map<cps_api_attr_id_t,
    cps_api_return_code_t (*)(BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_t, npu_id_t, cps_api_object_t)> _set_attr_handlers = {
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_MAC_AGE_TIMER, _set_generic_u32},
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_MODE, _set_generic_u32},
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_LAG_HASH_ALGORITHM, _set_generic_u32 },
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_ECMP_HASH_ALGORITHM, _set_generic_u32},
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_MAX_ECMP_ENTRY_PER_GROUP, _set_generic_u32 },
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_COUNTER_REFRESH_INTERVAL,_set_generic_u32},
        { BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_DEFAULT_MAC_ADDRESS, _set_generic_mac },
};


static void remove_same_values(cps_api_object_t now, cps_api_object_t req) {
    cps_api_object_it_t it;
    cps_api_object_it_begin(now,&it);
    for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it)) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);

        if (_set_attr_handlers.find(id)==_set_attr_handlers.end()) continue;

        cps_api_object_attr_t new_val = cps_api_object_e_get(req,&id,1);
        if (new_val==nullptr) continue;
        if (cps_api_object_attrs_compare(it.attr,new_val)!=0) continue;
        cps_api_object_attr_delete(req,id);
    }
}

static cps_api_return_code_t _switch_set(void * context,
                             cps_api_transaction_params_t * param, size_t ix) {

    cps_api_object_t obj = cps_api_object_list_get(param->change_list,ix);

    cps_api_operation_types_t op = cps_api_object_type_operation(cps_api_object_key(obj));

    if (op!=cps_api_oper_SET) {
        EV_LOGGING(NAS_L2,ERR,"NAS-L2-SWITCH","Invalid operation...");
        return cps_api_ret_code_ERR;
    }

    cps_api_object_attr_t _switch = cps_api_get_key_data(obj,BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_SWITCH_ID);
    if (_switch==NULL) {
        EV_LOGGING(NAS_L2,ERR,"NAS-L2-SWITCH","Invalid switch ID passed to set request...");

        return cps_api_ret_code_ERR;
    }

    BASE_CMN_LOGICAL_SWITCH_ID_t sw =(BASE_CMN_LOGICAL_SWITCH_ID_t)cps_api_object_attr_data_u32(_switch);
    const npu_id_t * npus = npu_list_from_switch(sw);
    if (npus==NULL || npu_list_from_switch_len(sw)==0) return cps_api_ret_code_ERR;

    cps_api_object_t prev = cps_api_object_list_create_obj_and_append(param->prev);
    if (prev==NULL) return cps_api_ret_code_ERR;

    _fill_obj_for_switch(prev, obj, (BASE_CMN_LOGICAL_SWITCH_ID_t)cps_api_object_attr_data_u32(_switch));

    remove_same_values(prev,obj);

    cps_api_object_it_t it;
    cps_api_object_it_begin(obj,&it);
    for ( ; cps_api_object_it_valid(&it); cps_api_object_it_next(&it)) {
        cps_api_attr_id_t id = cps_api_object_attr_id(it.attr);
        auto h =_set_attr_handlers.find(id);
        if (h==_set_attr_handlers.end()) continue;
        t_std_error rc = STD_ERR_OK;
        if ((rc=h->second((BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_t)id,
                *npus,obj))!=STD_ERR_OK) {
            EV_LOGGING(NAS_L2,ERR,"NAS-L2-SWITCH","Failed to set attr %d due to ndi error return (%d)",
                    (int)id,rc);
            return cps_api_ret_code_ERR;
        }
    }

    cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_OBSERVED);

    if (cps_api_event_thread_publish(obj)!=STD_ERR_OK) {
        EV_LOGGING(NAS_L2,ERR,"HAL-INTF-EVENT","Failed to send change event");
    }

    cps_api_key_set(cps_api_object_key(obj),CPS_OBJ_KEY_INST_POS,cps_api_qualifier_TARGET);

    return cps_api_ret_code_OK;
}

static t_std_error nas_switch_cps_init_entities(cps_api_operation_handle_t handle) {
    cps_api_registration_functions_t f;
    char buff[CPS_API_KEY_STR_MAX];
    memset(&f,0,sizeof(f));


    if (!cps_api_key_from_attr_with_qual(&_sws_ent_key,BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY,cps_api_qualifier_TARGET)) {
        EV_LOGGING(NAS_L2,ERR,"NAS-L2-SWITCH","Could not translate %d to key %s",
                            (int)(BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY),cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(INTERFACE,FAIL,0);
    }

    cps_api_key_copy(&f.key,&_sws_ent_key);

    EV_LOGGING(NAS_L2,INFO,"NAS-L2-SWITCH","Registering for %s",
                        cps_api_key_print(&f.key,buff,sizeof(buff)-1));
    f.handle = handle;
    f._read_function = _switch_get;
    f._write_function = _switch_set;
    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(CPSNAS,FAIL,0);
    }

    /* Don't return error if refresh interval is failed to set 5 seconds during init, it
     * should still continue and just log it
     */
    const npu_id_t  npu = 0;
    nas_ndi_switch_param_t param;
    auto _attr = BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY_COUNTER_REFRESH_INTERVAL;
    param.u32 = nas_def_counter_refresh_interval;
    if(ndi_switch_set_attribute(npu,_attr,&param)!=STD_ERR_OK){
        EV_LOGGING(NAS_L2,ERR,"NAS-SWITCH-CPS","Failed to initilaize the counter refresh interval to %d",
                nas_def_counter_refresh_interval);
    }
    return STD_ERR_OK;
}

static cps_api_return_code_t _global_switch_get (void * context, cps_api_get_params_t * param,
        size_t key_ix) {
    cps_api_object_t filt = cps_api_object_list_get(param->filters,key_ix);
    (void)filt;

    cps_api_object_t obj = create_obj_on_list(param->list,BASE_SWITCH_SWITCHING_ENTITIES_OBJ);
    if (obj==NULL) return cps_api_ret_code_ERR;

    //TODO look in config to determine how many switches exists (logical switches)
    cps_api_object_attr_add_u32(obj,BASE_SWITCH_SWITCHING_ENTITIES_SWITCH_COUNT,1);

    return cps_api_ret_code_OK;
}


static t_std_error nas_switch_cps_init_switch(cps_api_operation_handle_t handle) {
    cps_api_registration_functions_t f;
    char buff[CPS_API_KEY_STR_MAX];
    memset(&f,0,sizeof(f));

    if (!cps_api_key_from_attr_with_qual(&f.key,BASE_SWITCH_SWITCHING_ENTITIES_OBJ,cps_api_qualifier_TARGET)) {
        EV_LOGGING(NAS_L2,ERR,"NAS-L2-SWITCH","Could not translate %d to key %s",
                            (int)(BASE_SWITCH_SWITCHING_ENTITIES_SWITCHING_ENTITY),cps_api_key_print(&f.key,buff,sizeof(buff)-1));
        return STD_ERR(CPSNAS,FAIL,0);
    }
    EV_LOGGING(NAS_L2,INFO,"NAS-L2-SWITCH","Registering for %s",
                        cps_api_key_print(&f.key,buff,sizeof(buff)-1));
    f.handle = handle;
    f._read_function = _global_switch_get;
    if (cps_api_register(&f)!=cps_api_ret_code_OK) {
        return STD_ERR(CPSNAS,FAIL,0);
    }
    return STD_ERR_OK;
}

t_std_error nas_switch_cps_init(cps_api_operation_handle_t handle) {
    t_std_error rc = nas_switch_cps_init_switch(handle);
    if (rc==STD_ERR_OK) {
        rc = nas_switch_cps_init_entities(handle);
    }
    return rc;
}
