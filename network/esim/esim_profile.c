esim_result_errno_e ret = ESIM_INVALID_PARAM_ERR;
int num = 0;
osa_q_type_t profile_list;
esim_profile_info_t *profile_ptr = NULL;

osa_q_init(&profile_list);
ret = esim_intf_get_all_profile(&profile_list);
if(ret == ESIM_OK)
{
    num = profile_list.cnt;
    utils_printf("profile cnt:%d", num);
    for(i = 0; i < num; i++)
    {
        profile_ptr = osa_q_get(&profile_list);
        utils_printf("profile_ptr:%x", profile_ptr);
        if(profile_ptr != NULL)
        {
            utils_printf("\"%s\",%d,\"%s\",%d,\"%s\",\"%s\"", profile_ptr->iccid_info.data,
                                                              profile_ptr->status,
                                                              profile_ptr->nickname_info.data,
                                                              profile_ptr->pro_class,
                                                              profile_ptr->name_info.data,
                                                              profile_ptr->provider.data);
            osa_free(profile_ptr);
            profile_ptr = NULL;
        }
    }
}
osa_q_destroy(&profile_list);